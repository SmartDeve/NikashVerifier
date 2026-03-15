#pragma once
#include <string>
#include <cstdint>
#include <vector>

class Web3Core
{
public:
    explicit Web3Core(const char *rpcUrl);
    std::string sendTransaction(const std::string &to, uint64_t value, const std::string &data, uint64_t gasLimit);
     int determineRecId(const uint8_t *hash, const uint8_t *r_bytes, const uint8_t *s_bytes);
    std::string eth_call(const std::string &to, const std::string &data);
    std::string eth_getCode(const std::string &addr);
    void setPrivateKey(const uint8_t privkey[32]);
    std::string getAddress() const;
    std::string keccak256_utf8(const std::string &input);
    bool consume(uint64_t tokenId,
                 const std::string &contractAddress);
    uint64_t eth_getTransactionCount(const std::string &addr);
    std::string eth_sendRawTransaction(const std::string &rawTx);
    std::string consumeNFT(uint64_t tokenId, const std::string &contractAddr);
    std::string encodeUint(uint32_t v);
    std::string encodeAddress(const std::string &addr);
    std::string encodeBytes32(const std::string &hex);
    std::string encodeString(const std::string &s);

    uint32_t decodeUint(const std::string &hex, uint16_t word);
    bool decodeBool(const std::string &hex, uint16_t word);
    std::string decodeAddress(const std::string &hex, uint16_t word);
    std::string decodeString(const std::string &hex, uint16_t word);
    std::string sendEth(const std::string &toAddr, uint64_t amountInWei);

private:
    const char *_rpcUrl;
    uint8_t _privkey[32]{};
    uint8_t _address[20]{};

    /* ---------- INTERNAL HELPERS ---------- */
    void deriveAddress();

    std::string buildConsumeCalldata(uint64_t tokenId);
    std::string signLegacyTx(
        uint64_t nonce,
        uint64_t gasPrice,
        uint64_t gasLimit,
        const std::string &to,
        const std::string &data,
        uint64_t value);

    /* ---------- RLP ---------- */
    static std::string rlpEncodeBytes(const std::vector<uint8_t> &in);
    static std::string rlpEncodeUint(uint64_t v);
    static std::string rlpEncodeList(const std::vector<std::string> &items);
};
