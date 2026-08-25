# 🔐 NikashVerifier — Hardware Web3 Verifier

An **ESP32-based NFC verification terminal** that connects physical identity to on-chain credentials. The device reads an NFC card, verifies its associated account and ticket directly against an Ethereum smart contract, displays the result, and records successful usage on-chain.

---

## ✨ Highlights

- 📡 ESP32 + ESP-IDF + FreeRTOS
- 📱 PN532 NFC card interface
- 🔐 Keccak-256 card UID hashing
- 🌐 Ethereum JSON-RPC communication
- 📜 Smart contract ABI encoding/decoding
- 🎫 On-chain ticket verification
- 🔑 Private key storage using ESP32 NVS
- 🖥️ SH1106 128×64 OLED status display
- 💡 WS2812 RGB LED status indicators
- ⚡ FreeRTOS task and queue-based architecture
- ⛓️ On-chain ticket consumption after successful verification

---

## 🔄 Verification Flow

```text
        NFC Card
            │
            ▼
      PN532 Reader
            │
            ▼
       Card UID
            │
            ▼
       Keccak-256
            │
            ▼
   Account Registry
            │
            ▼
      Wallet Address
            │
            ▼
     Ticket Contract
            │
       ┌────┴────┐
       │         │
     Valid     Invalid
       │         │
       ▼         ▼
   🟢 GRANTED  🔴 DENIED
       │
       ▼
  Consume Ticket
       │
       ▼
 Ethereum Network
```

---

## 🧩 Hardware

| Component | Description |
|---|---|
| 🔲 ESP32 | Main microcontroller |
| 📡 PN532 NFC Reader | NFC card interface |
| 🖥️ SH1106 128×64 OLED | Status display |
| 💡 WS2812 RGB LED strip | Status indicators |
| 💳 NFC-compatible cards/tags | Physical credentials |

---

## 🛠️ Software Stack

`C++` · `ESP-IDF` · `FreeRTOS` · `PlatformIO` · `Ethereum` · `Web3` · `NFC` · `Cryptography`

The project uses ESP-IDF with PlatformIO and includes dedicated components for NFC communication, OLED display control, graphics rendering, and RGB LED control.

---

## 🔐 Security

- The verifier keeps the blockchain signing key on the device using ESP32 NVS storage and loads it into the Web3 layer when required.
- Temporary key material is cleared from memory after storage operations.

> Physical credentials. Verified by hardware. Recorded on-chain.

---

## 🎥 Demo

_Add demo video/GIF link here._

---

## 📁 Project Structure

```text
NikashVerifier/
├── components/
│   ├── led_strip/
│   ├── pn532/
│   ├── sh1106/
│   └── u8g2/
├── include/
├── lib/
│   └── ecc/
├── src/
│   └── main.cpp
├── test/
├── CMakeLists.txt
├── platformio.ini
└── partitions.csv
```

---

## 🚀 Build & Flash

### Requirements

- [ESP-IDF](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/)
- [PlatformIO](https://platformio.org/)
- ESP32 development board
- PN532 NFC reader
- SH1106 OLED

### Build

```bash
pio run
```

### Flash

```bash
pio run --target upload
```

### Monitor

```bash
pio device monitor
```

---

## ⚙️ Configuration

Before flashing the device, configure the required:

- 🌐 Ethereum RPC endpoint
- 📜 Smart contract addresses
- 🔑 Device signing key
- 📡 NFC and display hardware configuration

> ⚠️ Keep private keys and other sensitive credentials out of source control.

---

## ⛓️ Architecture

```text
┌─────────────────────────────────────┐
│            NikashVerifier           │
│                                     │
│  ┌─────────┐      ┌─────────────┐   │
│  │  PN532  │─────▶│ ESP32       │   │
│  │   NFC   │      │ Verification│    │
│  └─────────┘      │   Engine    │    │
│                    └──────┬──────┘   │
│                           │          │
│               ┌───────────┴──────┐   │
│               │                  │   │
│          ┌────▼────┐       ┌────▼──┐ │
│          │ SH1106  │       │WS2812 │ │
│          │ Display │       │  LED  │ │
│          └─────────┘       └───────┘ │
│                                       │
└────────────────┬──────────────────────┘
                  │
           Ethereum JSON-RPC
                  │
                  ▼
        ┌────────────────────┐
        │ Ethereum-compatible│
        │      Network       │
        └────────────────────┘
```

---

## 🎯 Use Case

NikashVerifier demonstrates how physical NFC credentials can be connected to blockchain-based verification infrastructure using a standalone embedded device.

The system combines:

**NFC → Embedded Verification → Cryptography → Smart Contracts → On-chain State**
