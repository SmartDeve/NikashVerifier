#define MBEDTLS_ALLOW_PRIVATE_ACCESS
#include "Web3Core.h"
#include "mbedtls/ecp.h"
#include "mbedtls/bignum.h"
#include <string>
#include <vector>
extern "C"
{
#include "uECC.h"
}

#include <cstring>
#include <cstdio>
#include "esp_random.h"
#include "esp_crt_bundle.h"
extern "C"
{
#include "esp_http_client.h"
#include "esp_log.h"
}
static int rng_callback(uint8_t *dest, unsigned size)
{
    while (size)
    {
        // esp_random() returns a 32-bit (4-byte) random number
        uint32_t val = esp_random();
        unsigned amount = (size < 4) ? size : 4;
        memcpy(dest, &val, amount);
        dest += amount;
        size -= amount;
    }
    return 1; // Return 1 to indicate success
}

// Helper to calculate RLP length prefix
// offset: 0x80 for bytes/strings, 0xC0 for lists
static std::string encodeLen(uint64_t len, uint8_t offset)
{
    char buf[10];
    if (len < 56)
    {
        // Short form: simple addition
        sprintf(buf, "%02x", (unsigned int)(len + offset));
        return std::string(buf);
    }
    else
    {
        // Long form: > 55 bytes
        // 1. Convert length to hex
        char lenHex[16];
        sprintf(lenHex, "%lx", (unsigned long)len);
        std::string lenStr = lenHex;

        // Ensure even number of hex digits (e.g., "F" -> "0F")
        if (lenStr.length() % 2 != 0)
            lenStr = "0" + lenStr;

        uint8_t bytesNeeded = lenStr.length() / 2;

        // 2. Prefix is (offset + 55 + bytesNeeded)
        sprintf(buf, "%02x", (unsigned int)(offset + 55 + bytesNeeded));

        return std::string(buf) + lenStr;
    }
}

// Helper: Strip leading zeros for RLP Integer encoding
static std::vector<uint8_t> stripZeros(const uint8_t *data, size_t len)
{
    size_t start = 0;
    // Find first non-zero byte
    while (start < len && data[start] == 0)
        start++;

    // If all zeros, return single zero byte
    if (start == len)
        return {0};

    return std::vector<uint8_t>(data + start, data + len);
}
// ---------- Constructor ----------
Web3Core::Web3Core(const char *rpcUrl)
{
    _rpcUrl = rpcUrl;
    uECC_set_rng(rng_callback);
}

// ---------- INTERNAL HTTP POST (ESP-IDF) ----------
#include "esp_http_client.h"
#include "esp_crt_bundle.h"

static std::string http_post(const char *url, const std::string &payload)
{

    esp_http_client_config_t config = {};
    config.url = url;
    config.method = HTTP_METHOD_POST;
    config.timeout_ms = 15000;
    config.crt_bundle_attach = esp_crt_bundle_attach;

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client)
        return "";

    esp_http_client_set_header(client, "Content-Type", "application/json");

    // 1️⃣ Open connection and tell ESP how much we’ll send
    esp_err_t err = esp_http_client_open(client, payload.length());
    if (err != ESP_OK)
    {
        ESP_LOGE("HTTP", "open failed: %s", esp_err_to_name(err));
        esp_http_client_cleanup(client);
        return "";
    }

    // 2️⃣ Write POST body
    int written = esp_http_client_write(
        client,
        payload.c_str(),
        payload.length());

    if (written <= 0)
    {
        ESP_LOGE("HTTP", "write failed");
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        return "";
    }

    // 3️⃣ Fetch headers (CRITICAL for chunked responses)
    int content_length = esp_http_client_fetch_headers(client);
    ESP_LOGI("HTTP", "content_length = %d", content_length);

    int status = esp_http_client_get_status_code(client);
    ESP_LOGI("HTTP", "status = %d", status);

    // 4️⃣ Read response body (loop!)
    std::string response;
    char buffer[256];

    while (true)
    {
        int r = esp_http_client_read(client, buffer, sizeof(buffer));
        if (r < 0)
        {
            ESP_LOGE("HTTP", "read error");
            response.clear();
            break;
        }
        if (r == 0)
        {
            break; // EOF
        }
        response.append(buffer, r);
    }

    esp_http_client_close(client);
    esp_http_client_cleanup(client);

    return response;
}

// ---------- RPC: eth_call ----------
std::string Web3Core::eth_call(const std::string &to,
                               const std::string &data)
{
    std::string payload =
        "{\"jsonrpc\":\"2.0\",\"method\":\"eth_call\",\"params\":[{"
        "\"to\":\"" +
        to + "\","
             "\"data\":\"" +
        data +
        "\"},\"latest\"],\"id\":1}";

    std::string resp = http_post(_rpcUrl, payload).c_str();

    if (resp.empty())
        return "";

    size_t p = resp.find("\"result\":\"");
    if (p == std::string::npos)
        return "";

    size_t start = p + 10;
    size_t end = resp.find("\"", start);

    std::string result = resp.substr(start, end - start);
    if (result.rfind("0x", 0) == 0)
        result = result.substr(2);

    return result;
}

// ---------- CRYPTO: keccak256 ----------
extern "C"
{
#include "keccak256.h"
}

std::string Web3Core::keccak256_utf8(const std::string &input)
{
    uint8_t hash[32];
    keccak256(
        (const uint8_t *)input.data(),
        input.length(),
        hash);

    char buf[3];
    std::string out = "0x";
    for (int i = 0; i < 32; i++)
    {
        sprintf(buf, "%02x", hash[i]);
        out += buf;
    }
    return out;
}

// ---------- ABI ENCODE ----------
std::string Web3Core::encodeUint(uint32_t v)
{
    char buf[65];
    sprintf(buf, "%064x", (unsigned int)v);
    return std::string(buf);
}

std::string Web3Core::encodeAddress(const std::string &addr)
{
    std::string a = addr;
    if (a.rfind("0x", 0) == 0)
        a = a.substr(2);

    return "000000000000000000000000" + a;
}

std::string Web3Core::encodeBytes32(const std::string &hex)
{
    std::string h = hex;
    if (h.rfind("0x", 0) == 0)
        h = h.substr(2);

    return h;
}

std::string Web3Core::encodeString(const std::string &s)
{
    std::string out = encodeUint(32);
    out += encodeUint(s.length());

    char buf[3];
    for (size_t i = 0; i < s.length(); i++)
    {
        sprintf(buf, "%02x", (uint8_t)s[i]);
        out += buf;
    }

    while (out.length() % 64 != 0)
        out += "0";

    return out;
}

// ---------- ABI DECODE ----------
uint32_t Web3Core::decodeUint(const std::string &hex, uint16_t word)
{
    int pos = word * 64 + 56;
    return strtoul(hex.substr(pos, 8).c_str(), nullptr, 16);
}

bool Web3Core::decodeBool(const std::string &hex, uint16_t word)
{
    return decodeUint(hex, word) != 0;
}

std::string Web3Core::decodeAddress(const std::string &hex, uint16_t word)
{
    int pos = word * 64 + 24;
    return "0x" + hex.substr(pos, 40);
}

// ---------- eth_getCode ----------
std::string Web3Core::eth_getCode(const std::string &addr)
{
    std::string payload =
        "{\"jsonrpc\":\"2.0\",\"method\":\"eth_getCode\","
        "\"params\":[\"" +
        addr + "\",\"latest\"],\"id\":1}";

    std::string resp = http_post(_rpcUrl, payload);
    if (resp.empty())
        return "";

    size_t p = resp.find("\"result\":\"");
    if (p == std::string::npos)
        return "";

    size_t start = p + 10;
    size_t end = resp.find("\"", start);
    return resp.substr(start, end - start);
}

std::string Web3Core::decodeString(const std::string &hex, uint16_t word)
{
    // 1. Culprit Fix: Handle "0x" prefix
    size_t skip = (hex.compare(0, 2, "0x") == 0) ? 2 : 0;

    // 2. Safety: Ensure we don't read past string length for the offset
    if (skip + (word * 64) + 64 > hex.length())
    {
        ESP_LOGE("WEB3", "Hex too short for word %d", word);
        return "";
    }

    // 3. Get the Offset
    uint32_t offset = strtoul(hex.substr(skip + (word * 64), 64).c_str(), nullptr, 16);

    // 4. Culprit Fix: Offset is in bytes, convert to hex chars and add skip
    size_t lengthPos = skip + (offset * 2);

    if (lengthPos + 64 > hex.length())
    {
        ESP_LOGE("WEB3", "Hex too short for length at offset %d", offset);
        return "";
    }

    // 5. Get the Length
    uint32_t length = strtoul(hex.substr(lengthPos, 64).c_str(), nullptr, 16);

    // 6. Decode the characters
    std::string out;
    out.reserve(length);
    size_t dataStart = lengthPos + 64;

    for (uint32_t i = 0; i < length; i++)
    {
        if (dataStart + (i * 2) + 2 > hex.length())
            break;

        // Convert 2 hex chars to 1 byte
        std::string byteHex = hex.substr(dataStart + (i * 2), 2);
        out += (char)strtoul(byteHex.c_str(), nullptr, 16);
    }

    return out;
}

void Web3Core::setPrivateKey(const uint8_t privkey[32])
{
    memcpy(_privkey, privkey, 32);
    deriveAddress();
}

void Web3Core::deriveAddress()
{
    uint8_t pubkey[64];
    uECC_compute_public_key(
        _privkey,
        pubkey,
        uECC_secp256k1());

    uint8_t hash[32];
    keccak256(pubkey, 64, hash);

    memcpy(_address, hash + 12, 20);
}

std::string Web3Core::getAddress() const
{
    static const char hex[] = "0123456789abcdef";
    std::string out = "0x";
    for (int i = 0; i < 20; i++)
    {
        out += hex[_address[i] >> 4];
        out += hex[_address[i] & 0x0F];
    }
    return out;
}

uint64_t Web3Core::eth_getTransactionCount(const std::string &addr)
{
    std::string payload =
        "{\"jsonrpc\":\"2.0\",\"method\":\"eth_getTransactionCount\","
        "\"params\":[\"" +
        addr + "\",\"pending\"],\"id\":1}";

    std::string resp = http_post(_rpcUrl, payload);

    // Debug Log: Uncomment to see exactly what you get
    // ESP_LOGI("Web3", "Nonce Resp: %s", resp.c_str());

    // 1. Find the key "result"
    size_t resultPos = resp.find("\"result\"");
    if (resultPos == std::string::npos)
        return 0;

    // 2. Find the opening quote for the value
    // We search AFTER resultPos
    size_t startQuote = resp.find("\"", resultPos + 8);
    if (startQuote == std::string::npos)
        return 0;

    // 3. Check for "0x" prefix
    // The value starts at startQuote + 1.
    // We check if the characters at startQuote+1 and startQuote+2 are '0' and 'x'
    size_t valueStart = startQuote + 1;
    if (resp.substr(valueStart, 2) == "0x")
    {
        valueStart += 2; // Skip the "0x" so we point to "1d"
    }

    // 4. Find the closing quote
    size_t endQuote = resp.find("\"", valueStart);
    if (endQuote == std::string::npos)
        return 0;

    // 5. Extract just the hex digits (e.g., "1d")
    std::string hexVal = resp.substr(valueStart, endQuote - valueStart);

    // 6. Convert to integer
    return strtoull(hexVal.c_str(), nullptr, 16);
}

std::string Web3Core::buildConsumeCalldata(uint64_t tokenId)
{
    // selector = 0x483f31ab
    return "0x483f31ab" + encodeUint(tokenId);
}

std::string Web3Core::rlpEncodeUint(uint64_t v)
{
    if (v == 0)
        return "80";

    std::vector<uint8_t> bytes;
    while (v)
    {
        bytes.insert(bytes.begin(), v & 0xff);
        v >>= 8;
    }
    return rlpEncodeBytes(bytes);
}

std::string Web3Core::rlpEncodeBytes(const std::vector<uint8_t> &in)
{
    std::string out;

    // Special Case: Single byte < 0x80 is its own encoding
    if (in.size() == 1 && in[0] < 0x80)
    {
        char buf[5];
        sprintf(buf, "%02x", in[0]);
        return std::string(buf);
    }

    // Standard Case: Calculate Prefix (0x80 base)
    out = encodeLen(in.size(), 0x80);

    // Append actual bytes
    char buf[5];
    for (auto b : in)
    {
        sprintf(buf, "%02x", b);
        out += buf;
    }
    return out;
}

std::string Web3Core::rlpEncodeList(const std::vector<std::string> &items)
{
    std::string payload;
    for (const auto &i : items)
        payload += i;

    // Length in bytes = hex chars / 2
    uint64_t len = payload.length() / 2;

    // Use helper to generate correct prefix (0xC0 base)
    std::string prefix = encodeLen(len, 0xc0);

    return prefix + payload;
}
std::string Web3Core::eth_sendRawTransaction(const std::string &rawTx)
{
    std::string payload = "{\"jsonrpc\":\"2.0\",\"method\":\"eth_sendRawTransaction\",\"params\":[\"" + rawTx + "\"],\"id\":1}";

    std::string resp = http_post(_rpcUrl, payload);

    // Simple parse for result (Transaction Hash)
    size_t p = resp.find("\"result\":\"");
    if (p == std::string::npos)
    {
        ESP_LOGE("Web3", "Tx Failed: %s", resp.c_str());
        return "";
    }

    size_t start = p + 10;
    size_t end = resp.find("\"", start);
    return resp.substr(start, end - start);
}
// Helper to convert hex string (with or without 0x) to byte vector
static std::vector<uint8_t> hexToBytes(const std::string &hex)
{
    std::vector<uint8_t> bytes;
    size_t start = (hex.rfind("0x", 0) == 0) ? 2 : 0;

    for (size_t i = start; i < hex.length(); i += 2)
    {
        std::string byteString = hex.substr(i, 2);
        uint8_t byte = (uint8_t)strtol(byteString.c_str(), nullptr, 16);
        bytes.push_back(byte);
    }
    return bytes;
}
std::string Web3Core::signLegacyTx(
    uint64_t nonce,
    uint64_t gasPrice,
    uint64_t gasLimit,
    const std::string &to,
    const std::string &data,
    uint64_t value)
{
    // IMPORTANT: Ensure this matches your network!
    // Sepolia = 11155111. Mainnet = 1. Ganache/Local = 1337.
    uint64_t chainId = 11155111;

    // 1. Prepare Raw Fields
    std::vector<uint8_t> toBytes = hexToBytes(to);
    std::vector<uint8_t> dataBytes = hexToBytes(data);

    // 2. Build Pre-Image RLP List (EIP-155)
    std::vector<std::string> txFields;
    txFields.push_back(rlpEncodeUint(nonce));
    txFields.push_back(rlpEncodeUint(gasPrice));
    txFields.push_back(rlpEncodeUint(gasLimit));
    txFields.push_back(rlpEncodeBytes(toBytes));
    txFields.push_back(rlpEncodeUint(value));
    txFields.push_back(rlpEncodeBytes(dataBytes));
    txFields.push_back(rlpEncodeUint(chainId));
    txFields.push_back("80"); // empty r (represented as 0x80 or empty string)
    txFields.push_back("80"); // empty s

    std::string unsignedTxRLP = rlpEncodeList(txFields);

    // 3. Hash the RLP
    // --- FIX START ---
    uint8_t hash[32];
    std::vector<uint8_t> rawTxBytes = hexToBytes(unsignedTxRLP);
    keccak256(rawTxBytes.data(), rawTxBytes.size(), hash);
    // --- FIX END ---

    uint8_t signature[64];
    int recid = 0;

    // 4. Sign Loop
    for (int i = 0; i < 5; i++)
    {
        if (!uECC_sign(_privkey, hash, sizeof(hash), signature, uECC_secp256k1()))
        {
            ESP_LOGE("Web3", "Signing failed");
            return "";
        }

        // Check High-S (EIP-2)
        if (signature[32] > 0x7F)
            continue;

        // Use our new local mbedtls method
        recid = determineRecId(hash, signature, signature + 32);

        break;
    }

    uint8_t *r_bytes = signature;
    uint8_t *s_bytes = signature + 32;

    // 5. Calculate v
    // EIP-155: v = chainId * 2 + 35 + recid
    uint64_t v_val = (chainId * 2) + 35 + recid;

    // 6. Build Signed RLP List
    std::vector<std::string> signedFields;
    signedFields.push_back(rlpEncodeUint(nonce));
    signedFields.push_back(rlpEncodeUint(gasPrice));
    signedFields.push_back(rlpEncodeUint(gasLimit));
    signedFields.push_back(rlpEncodeBytes(toBytes));
    signedFields.push_back(rlpEncodeUint(value));
    signedFields.push_back(rlpEncodeBytes(dataBytes));
    signedFields.push_back(rlpEncodeUint(v_val));
    signedFields.push_back(rlpEncodeBytes(stripZeros(r_bytes, 32)));
    signedFields.push_back(rlpEncodeBytes(stripZeros(s_bytes, 32)));

    std::string signedTxRLP = rlpEncodeList(signedFields);

    return "0x" + signedTxRLP;
}

// Helper to try recovering the public key and checking if it matches ours
int Web3Core::determineRecId(const uint8_t *hash, const uint8_t *r_bytes, const uint8_t *s_bytes)
{
    mbedtls_ecp_group grp;
    mbedtls_ecp_point R, P;
    mbedtls_mpi r, s, z, inv_s, u1, u2;

    // --- MOVED UP: Declare these here to avoid "transfer of control" error ---
    size_t olen = 0;
    uint8_t R_bin[65];
    // -----------------------------------------------------------------------

    mbedtls_ecp_group_init(&grp);
    mbedtls_ecp_point_init(&R);
    mbedtls_ecp_point_init(&P);
    mbedtls_mpi_init(&r);
    mbedtls_mpi_init(&s);
    mbedtls_mpi_init(&z);
    mbedtls_mpi_init(&inv_s);
    mbedtls_mpi_init(&u1);
    mbedtls_mpi_init(&u2);

    int ret = 0;
    int recid = 0;

    // 1. Load Curve
    MBEDTLS_MPI_CHK(mbedtls_ecp_group_load(&grp, MBEDTLS_ECP_DP_SECP256K1));

    // 2. Import Public Key
    {
        uint8_t pub[64];
        uECC_compute_public_key(_privkey, pub, uECC_secp256k1());

        uint8_t fullPub[65];
        fullPub[0] = 0x04;
        memcpy(fullPub + 1, pub, 64);

        MBEDTLS_MPI_CHK(mbedtls_ecp_point_read_binary(&grp, &P, fullPub, 65));
    }

    // 3. Import Signature & Hash
    MBEDTLS_MPI_CHK(mbedtls_mpi_read_binary(&r, r_bytes, 32));
    MBEDTLS_MPI_CHK(mbedtls_mpi_read_binary(&s, s_bytes, 32));
    MBEDTLS_MPI_CHK(mbedtls_mpi_read_binary(&z, hash, 32));

    // 4. Calculate R = s^-1 * (z*G + r*P)
    MBEDTLS_MPI_CHK(mbedtls_mpi_inv_mod(&inv_s, &s, &grp.N)); // inv_s = s^-1
    MBEDTLS_MPI_CHK(mbedtls_mpi_mul_mpi(&u1, &z, &inv_s));    // u1 = z * inv_s
    MBEDTLS_MPI_CHK(mbedtls_mpi_mod_mpi(&u1, &u1, &grp.N));
    MBEDTLS_MPI_CHK(mbedtls_mpi_mul_mpi(&u2, &r, &inv_s)); // u2 = r * inv_s
    MBEDTLS_MPI_CHK(mbedtls_mpi_mod_mpi(&u2, &u2, &grp.N));
    MBEDTLS_MPI_CHK(mbedtls_ecp_muladd(&grp, &R, &u1, &grp.G, &u2, &P)); // R = u1*G + u2*P

    // 5. Determine Parity of Y
    // Now we can safely use R_bin because it was declared at the top
    MBEDTLS_MPI_CHK(mbedtls_ecp_point_write_binary(&grp, &R, MBEDTLS_ECP_PF_UNCOMPRESSED, &olen, R_bin, sizeof(R_bin)));

    if (R_bin[64] & 1)
    {
        recid = 1;
    }
    else
    {
        recid = 0;
    }

cleanup:
    mbedtls_ecp_group_free(&grp);
    mbedtls_ecp_point_free(&R);
    mbedtls_ecp_point_free(&P);
    mbedtls_mpi_free(&r);
    mbedtls_mpi_free(&s);
    mbedtls_mpi_free(&z);
    mbedtls_mpi_free(&inv_s);
    mbedtls_mpi_free(&u1);
    mbedtls_mpi_free(&u2);

    if (ret != 0)
    {
        ESP_LOGE("Web3", "mbedtls math failed: -0x%04x", -ret);
        return 0;
    }

    return recid;
}
// Generic function for ANY state-changing transaction
std::string Web3Core::sendTransaction(const std::string &to, uint64_t value, const std::string &data, uint64_t gasLimit)
{
    // 1. Get Address & Nonce
    std::string myAddr = getAddress();
    uint64_t nonce = eth_getTransactionCount(myAddr);

    ESP_LOGI("Web3", "Tx Prep - Addr: %s, Nonce: %llu", myAddr.c_str(), nonce);

    if (nonce == UINT64_MAX)
    {
        ESP_LOGE("Web3", "Failed to get nonce");
        return "";
    }

    // 2. Gas Price (Hardcoded 3 Gwei - adjust if needed)
    uint64_t gasPrice = 9000000000;

    // 3. Sign the Transaction
    // This uses your existing signLegacyTx which we know works!
    std::string signedTx = signLegacyTx(nonce, gasPrice, gasLimit, to, data, value);

    if (signedTx.empty())
    {
        ESP_LOGE("Web3", "Signing failed");
        return "";
    }

    // 4. Broadcast
    return eth_sendRawTransaction(signedTx);
}
std::string
Web3Core::sendEth(const std::string &toAddr, uint64_t amountInWei)
{
    ESP_LOGI("Web3", "Preparing to send %llu wei to %s", amountInWei, toAddr.c_str());

    // 1. Get current Nonce
    std::string myAddr = getAddress();

    uint64_t nonce = eth_getTransactionCount(myAddr);
    ESP_LOGI("Web3", "My Local Address: %s", myAddr.c_str());
    ESP_LOGI("Web3", "Fetched Nonce: %llu", nonce);
    if (nonce == UINT64_MAX)
    {
        ESP_LOGE("Web3", "Failed to get nonce");
        return "";
    }

    // 2. Standard Gas Params for a simple transfer
    // A simple transfer always requires exactly 21000 Gas
    uint64_t gasLimit = 21000;

    // Hardcoded Gas Price (e.g., 5 Gwei).
    // On a real mainnet, you should fetch this via eth_gasPrice
    uint64_t gasPrice = 8000000000;

    // 3. Sign Transaction
    // Data is empty ("") for a standard ETH transfer
    std::string signedTx = signLegacyTx(nonce, gasPrice, gasLimit, toAddr, "", amountInWei);

    if (signedTx.empty())
    {
        ESP_LOGE("Web3", "Signing failed");
        return "";
    }

    // 4. Broadcast
    std::string txHash = eth_sendRawTransaction(signedTx);

    if (!txHash.empty())
    {
        ESP_LOGI("Web3", "ETH Transfer Sent! Hash: %s", txHash.c_str());
    }

    return txHash;
}
std::string Web3Core::consumeNFT(uint64_t tokenId, const std::string &contractAddr)
{
    ESP_LOGI("Web3", "Consuming Ticket ID: %llu", tokenId);

    // 1. Selector for consume(uint256) -> 0x483f31ab
    // You already calculated this correctly!
    std::string selector = "0x483f31ab";

    // 2. Encode Parameter (uint256 tokenId)
    // Must be padded to 32 bytes (64 hex chars)
    // We use your encodeUint helper which does exactly this.
    std::string param = encodeUint(tokenId);

    // 3. Combine
    std::string data = selector + param;

    // 4. Send Transaction
    // Value = 0 (No ETH sent)
    // Data = selector + param
    // Gas Limit = 100,000 (Safe buffer for state updates)
    return sendTransaction(contractAddr, 0, data, 100000);
}
// Generic function for ANY state-changing transaction
