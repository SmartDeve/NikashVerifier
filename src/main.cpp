extern "C"
{
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
}

typedef enum
{
    LED_STATE_WIFI_DISCONNECTED,
    LED_STATE_WIFI_CONNECTED,
    LED_STATE_WEB3_CHECKING,
    LED_STATE_WEB3_SUCCESS,
    LED_STATE_WEB3_FAILED
} led_state_t;

volatile led_state_t current_led_state = LED_STATE_WIFI_DISCONNECTED;

#include "Web3Core.h"
#include "string.h"
#include "led_strip.h"
#include "u8g2.h"
#include <algorithm>
#include "esp_wifi.h"
#include "driver/i2c.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ssd1306.h" #include "esp_netif.h"
#include "pn532.h"
#include "pn532_driver_i2c.h"
#include "pn532_driver_hsu.h"
#include "pn532_driver_spi.h"
#include "esp_system.h"

// ===== NVS (required for WiFi) =====
#include "nvs_flash.h"
#define GET_ACCOUNT "0xd1de5011"
#define PN532_RST_GPIO 25
#define GET_TICKET_ID "0x95ef1aeb"
#define GET_TICKET_DETAILS "0x24c14160"
#define CONSUME "0x483f31ab"
#define PN532_MODE_I2C 1
#define PN532_MODE_HSU 0
#define PN532_MODE_SPI 0
#define RESET_PIN (-1)
#define IRQ_PIN (6)
// #define IRQ_PIN        (-1)
#define SPI_CS GPIO_NUM_5
#define SPI_SCK GPIO_NUM_18
#define SPI_MISO GPIO_NUM_19
#define SPI_MOSI GPIO_NUM_23
#define SPI_HOST_NFC (SPI3_HOST)
#define SPI_CLOCKRATE (1000000)
#include <math.h>

#define NUM_LEDS 16
#define IRQ_PIN GPIO_NUM_NC
#define OLED_ADDR 0x3C
#define RESET_PIN GPIO_NUM_NC
static const char *ACCOUNT_REGISTRY =
    "0xe7d2B6fc4E2F2545c08F03637b39f182D8209b2F";

static const char *TICKET_CONTRACT =
    "0x75d226d0034eeF025e33a9669Ee09470eA1f8d26";

static const char *RPC_URL =
    "https://eth-sepolia.g.alchemy.com/v2/rWlpaN197HdUaILDb0_UJjil5KpOrR6x";

static EventGroupHandle_t wifi_event_group;

#define I2C_PORT I2C_NUM_0
#define SDA_PIN 21
#define SCL_PIN 22
#define OLED_ADDR 0x3C

#define LCD_H_RES 128
#define LCD_V_RES 64

static esp_lcd_panel_handle_t panel_handle = NULL;

#define WIFI_CONNECTED_BIT BIT0
#define KEY_LOADED_BIT BIT1

#define I2C_SDA 21
#define I2C_SCL 22
#define I2C_PORT I2C_NUM_0
#define OLED_ADDR 0x3C

static const char *WIFI_TAG = "WIFI";
static const char *TAG = "MAIN";
static const char *W3TAG = "WEB3";
#define WIFI_SSID "Parai Likhai Karo Yaar"
#define WIFI_PASS "HariBol01#"
EventGroupHandle_t appEventGroup;
#define PRIVKEY_LEN 32
#include "freertos/queue.h"

pn532_io_t pn532io;
i2c_master_bus_handle_t i2c_bus_handle;
i2c_master_dev_handle_t i2c_dev_handle;

typedef struct
{
    char contractAddress[43]; // 0x + 40 chars + null terminator
    uint64_t tokenID;         // Amount in Wei
} tx_msg_t;

typedef struct
{
    char text[32];
    int top_padding;
} display_msg_t;

// 2. Declare the Queue Handle
QueueHandle_t tx_queue;
QueueHandle_t display_queue;
QueueHandle_t key_queue;
Web3Core web3(RPC_URL);
typedef struct
{
    uint8_t key[PRIVKEY_LEN];
} privkey_msg_t;

static void wifi_event_handler(void *arg,
                               esp_event_base_t event_base,
                               int32_t event_id,
                               void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START)
    {
        ESP_LOGI(WIFI_TAG, "WiFi started, connecting...");
        esp_wifi_connect();
    }

    else if (event_base == WIFI_EVENT &&
             event_id == WIFI_EVENT_STA_DISCONNECTED)
    {
        ESP_LOGW(WIFI_TAG, "Disconnected, retrying...");
        xEventGroupClearBits(wifi_event_group, WIFI_CONNECTED_BIT);
        current_led_state = LED_STATE_WIFI_DISCONNECTED;
        esp_wifi_connect();
    }

    else if (event_base == IP_EVENT &&
             event_id == IP_EVENT_STA_GOT_IP)
    {
        ESP_LOGI(WIFI_TAG, "Got IP, WiFi connected");
        xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_BIT);
        current_led_state = LED_STATE_WIFI_CONNECTED;
    }
}

#include "driver/i2c_master.h"

#define I2C_SDA 21
#define I2C_SCL 22
#define I2C_PORT I2C_NUM_0
#define OLED_ADDR 0x3C
void uid_to_string(uint8_t *uid, uint8_t length, char *output)
{
    for (int i = 0; i < length; i++)
    {
        sprintf(&output[i * 2], "%02x", uid[i]);
    }
    output[length * 2] = '\0'; // Null terminate
}
u8g2_t u8g2;
void display_task(void *pv)
{
    display_msg_t msg;

    while (1)
    {
        if (xQueueReceive(display_queue, &msg, portMAX_DELAY))
        {
            u8g2_ClearBuffer(&u8g2);
            

            int ascent = u8g2_GetAscent(&u8g2);
            int descent = u8g2_GetDescent(&u8g2);
            int line_height = ascent - descent;
            int spacing = 4;

            // Count lines
            int lines = 1;
            for (const char *p = msg.text; *p; p++)
                if (*p == '\n')
                    lines++;

            if(lines>1)
            {
                u8g2_SetFont(&u8g2, u8g2_font_helvB08_tr);
            }
            else
            {
                u8g2_SetFont(&u8g2, u8g2_font_helvB12_tr);
            }
            int total_height = lines * line_height + (lines - 1) * spacing;

            // Start Y so entire block is vertically centered
            int y = (64 - total_height) / 2 + ascent;

            const char *start = msg.text;

            while (*start)
            {
                char line[32];
                int i = 0;

                // Extract one line
                while (*start && *start != '\n' && i < sizeof(line) - 1)
                    line[i++] = *start++;

                line[i] = '\0';

                if (*start == '\n')
                    start++;

                // Horizontal center for this line
                int width = u8g2_GetStrWidth(&u8g2, line);
                int x = (128 - width) / 2;

                u8g2_DrawStr(&u8g2, x, y, line);

                y += line_height + spacing;
            }

            u8g2_SendBuffer(&u8g2);
        }
    }
}

void display_text(const char *text, int padding)
{
    display_msg_t msg;

    strncpy(msg.text, text, sizeof(msg.text) - 1);
    msg.text[sizeof(msg.text) - 1] = '\0';
    msg.top_padding = padding;

    xQueueSend(display_queue, &msg, portMAX_DELAY);
}
void i2c_init(void)
{
    // Bus configuration
    i2c_master_bus_config_t bus_config = {};
    bus_config.i2c_port = I2C_PORT;
    bus_config.sda_io_num = GPIO_NUM_21;
    bus_config.scl_io_num = GPIO_NUM_22;
    bus_config.clk_source = I2C_CLK_SRC_DEFAULT;
    bus_config.glitch_ignore_cnt = 7;
    bus_config.intr_priority = 0;
    bus_config.trans_queue_depth = 0;
    bus_config.flags.enable_internal_pullup = true;

    ESP_ERROR_CHECK(i2c_new_master_bus(&bus_config, &i2c_bus_handle));

    // Device configuration
    i2c_device_config_t dev_config = {};
    dev_config.dev_addr_length = I2C_ADDR_BIT_LEN_7;
    dev_config.device_address = OLED_ADDR;
    dev_config.scl_speed_hz = 400000;

    ESP_ERROR_CHECK(
        i2c_master_bus_add_device(
            i2c_bus_handle,
            &dev_config,
            &i2c_dev_handle));
}

uint8_t u8x8_byte_esp32_hw_i2c(u8x8_t *u8x8,
                               uint8_t msg,
                               uint8_t arg_int,
                               void *arg_ptr)
{
    static uint8_t buffer[32];
    static uint8_t idx;

    switch (msg)
    {
    case U8X8_MSG_BYTE_INIT:
        break;

    case U8X8_MSG_BYTE_SEND:
        memcpy(&buffer[idx], arg_ptr, arg_int);
        idx += arg_int;
        break;

    case U8X8_MSG_BYTE_START_TRANSFER:
        idx = 0;
        break;

    case U8X8_MSG_BYTE_END_TRANSFER:
        ESP_ERROR_CHECK(
            i2c_master_transmit(
                i2c_dev_handle,
                buffer,
                idx,
                -1 // wait forever
                ));
        break;
    default:
        return 0;
    }
    return 1;
}
uint8_t u8x8_gpio_and_delay_esp32(u8x8_t *u8x8,
                                  uint8_t msg,
                                  uint8_t arg_int,
                                  void *arg_ptr)
{
    switch (msg)
    {
    case U8X8_MSG_DELAY_MILLI:
        vTaskDelay(pdMS_TO_TICKS(arg_int));
        break;
    default:
        break;
    }
    return 1;
}
void wifi_init()
{

    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(
        esp_event_handler_register(
            WIFI_EVENT,
            ESP_EVENT_ANY_ID,
            &wifi_event_handler,
            nullptr));

    ESP_ERROR_CHECK(
        esp_event_handler_register(
            IP_EVENT,
            IP_EVENT_STA_GOT_IP,
            &wifi_event_handler,
            nullptr));

    wifi_config_t wifi_config = {};
    strcpy((char *)wifi_config.sta.ssid, WIFI_SSID);
    strcpy((char *)wifi_config.sta.password, WIFI_PASS);

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
}

void init_nvs(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES ||
        err == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        nvs_flash_erase();
        nvs_flash_init();
    }
}

void nfcReadTask(void *params)
{

    vTaskDelay(pdMS_TO_TICKS(500));
}

void private_key_storage_task(void *arg)
{

    privkey_msg_t msg;
    nvs_handle_t nvs;

    ESP_ERROR_CHECK(
        nvs_open("keys", NVS_READWRITE, &nvs));

    while (1)
    {
        if (xQueueReceive(key_queue, &msg, portMAX_DELAY))
        {
            ESP_LOGI("KEY", "Writing private key to NVS");
            ESP_ERROR_CHECK(nvs_set_blob(nvs, "wallet_priv", msg.key, PRIVKEY_LEN));
            ESP_ERROR_CHECK(nvs_commit(nvs));
            memset(msg.key, 0, PRIVKEY_LEN);
        }
    }
}

void web3_task(void *params)
{
    ESP_LOGI(W3TAG, "Web3 task started");
    current_led_state = LED_STATE_WEB3_CHECKING;

    display_text("Verifying...", 12);
    char *cardHashRecieved = (char *)params;
    ESP_LOGI(W3TAG, "WEB3 UID: %s", cardHashRecieved);
    std::string myAddress = web3.getAddress();
    tx_msg_t txMsg;
    strncpy(txMsg.contractAddress, TICKET_CONTRACT, sizeof(txMsg.contractAddress) - 1);
    txMsg.contractAddress[sizeof(txMsg.contractAddress) - 1] = '\0'; // Safety null-terminate

    Web3Core web3(RPC_URL);

    // ---- CARD HASH ----
    std::string cardHash =
        web3.keccak256_utf8(cardHashRecieved);

    // ---- GET ACCOUNT ----
    std::string data =
        GET_ACCOUNT + web3.encodeBytes32(cardHash);

    std::string result =
        web3.eth_call(ACCOUNT_REGISTRY, data);

    if (result.empty())
    {
        ESP_LOGE(W3TAG, "eth_call failed (account)");
        vTaskDelete(nullptr);
    }

    std::string wallet = web3.decodeAddress(result, 0);

    std::string name = web3.decodeString(result, 3);

    // ---- GET TICKET ID ----
    data = GET_TICKET_ID + web3.encodeAddress(wallet);
    result = web3.eth_call(TICKET_CONTRACT, data);

    uint32_t ticketToken = web3.decodeUint(result, 0);
    txMsg.tokenID = ticketToken;
    if (ticketToken == 0)
    {

        ESP_LOGE(W3TAG, "ACCESS DENIED, NO SEAL!");
        display_text("NO SEAL", 12);
        current_led_state = LED_STATE_WEB3_FAILED;
        vTaskDelete(nullptr);
    }
    // ---- GET TICKET DETAILS ----
    data = GET_TICKET_DETAILS + web3.encodeUint(ticketToken);
    result = web3.eth_call(TICKET_CONTRACT, data);

    uint8_t ticketType = web3.decodeUint(result, 0);
    std::string seat = web3.decodeString(result, 1);
    uint32_t validFrom = web3.decodeUint(result, 2);
    uint32_t validTo = web3.decodeUint(result, 3);
    bool used = web3.decodeBool(result, 4);
    std::string owner = web3.decodeAddress(result, 5);

    // ESP_LOGI(W3TAG, "Name   : %s", name.c_str());
    // ESP_LOGI(W3TAG, "Wallet : %s", wallet.c_str());
    // ESP_LOGI(W3TAG, "ticketType: %u", ticketType);
    // ESP_LOGI(W3TAG, "Ticket token: %lu", (unsigned long)ticketToken);
    // ESP_LOGI(W3TAG, "seat      : %s", seat.c_str());
    // ESP_LOGI(W3TAG, "validFrom : %lu", (unsigned long)validFrom);
    // ESP_LOGI(W3TAG, "validTo   : %lu", (unsigned long)validTo);
    // ESP_LOGI(W3TAG, "used      : %d", used);
    // ESP_LOGI(W3TAG, "owner     : %s", owner.c_str());

    std::string access = "GRANTED";

    if (!used)
    {
        ESP_LOGI(W3TAG, "Name   : %s", name.c_str());
        ESP_LOGI(W3TAG, "ACCESS   : %s", access.c_str());
        char buffer[128];

        snprintf(buffer, sizeof(buffer),
                 "Name:%s\nAccess:%s",
                 name.c_str(),
                 access.c_str());

        display_text(buffer, 12);

        current_led_state = LED_STATE_WEB3_SUCCESS;
        if (xQueueSend(tx_queue, &txMsg, 0) == pdTRUE)
        {
            ESP_LOGI(W3TAG, "Transaction queued successfully");
        }
        else
        {
            ESP_LOGW(W3TAG, "Tx Queue full! Dropping request.");
        }
        vTaskDelete(nullptr);
    }

    access = "DENIED";

    ESP_LOGI(W3TAG, "Name   : %s", name.c_str());
    ESP_LOGI(W3TAG, "ACCESS   : %s", access.c_str());
    current_led_state = LED_STATE_WEB3_FAILED;
    char buffer[128];
    snprintf(buffer, sizeof(buffer),
             "Name:%s\nAccess:%s",
             name.c_str(),
             access.c_str());

    display_text(buffer, 12);

    vTaskDelete(nullptr);
}

static uint8_t hex_nibble(char c)
{
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    return 0;
}

void hex_to_bytes_32(const char *hex, uint8_t out[32])
{
    for (int i = 0; i < 32; i++)
    {
        out[i] =
            (hex_nibble(hex[i * 2]) << 4) |
            hex_nibble(hex[i * 2 + 1]);
    }
}

esp_err_t load_private_key(privkey_msg_t *out)
{

    if (!out)
    {
        return ESP_ERR_INVALID_ARG;
    }

    nvs_handle_t nvs;
    size_t len = PRIVKEY_LEN;

    esp_err_t err = nvs_open("keys", NVS_READONLY, &nvs);
    if (err != ESP_OK)
    {
        ESP_LOGE("NVS", "Failed to open NVS: %s", esp_err_to_name(err));
        return err;
    }

    err = nvs_get_blob(nvs, "wallet_priv", out->key, &len);
    nvs_close(nvs);

    if (err != ESP_OK)
    {
        ESP_LOGE("NVS", "Failed to read privkey: %s", esp_err_to_name(err));
        return err;
    }

    if (len != PRIVKEY_LEN)
    {
        ESP_LOGE("NVS", "Invalid privkey length: %d", len);
        return ESP_ERR_INVALID_SIZE;
    }

    ESP_LOGI("NVS", "Private key loaded successfully");
    return ESP_OK;
}
void bytes_to_hex_32(const uint8_t in[32], char out[65])
{
    static const char hex[] = "0123456789abcdef";

    for (int i = 0; i < 32; i++)
    {
        out[i * 2] = hex[in[i] >> 4];
        out[i * 2 + 1] = hex[in[i] & 0x0F];
    }

    out[64] = '\0'; // null-terminate
}

void retrieve_private_key(void *params)
{
    privkey_msg_t keys;
    appEventGroup = xEventGroupCreate();
    esp_err_t err = load_private_key(&keys);
    if (err != ESP_OK)
    {
        ESP_LOGE("KEY", "Failed to load private key");
        return;
    }

    char hex_out[65]; // 32 bytes → 64 hex chars + '\0'
    bytes_to_hex_32(keys.key, hex_out);
    web3.setPrivateKey(keys.key);

    xEventGroupSetBits(wifi_event_group, KEY_LOADED_BIT);

    vTaskDelete(NULL);
}

void build_consume_calldata(uint64_t tokenId, char out[2 + 8 + 64 + 1])
{
    // 0x + selector (8 hex) + uint256 (64 hex)
    sprintf(
        out,
        "0x%s%064llx",
        CONSUME,
        (unsigned long long)tokenId);
}

void tx_worker_task(void *params)
{
    ESP_LOGI("TX_WORKER", "Transaction Worker Started");

    tx_msg_t msg;

    while (1)
    {
        // 1. Wait indefinitely for a message to arrive
        if (xQueueReceive(tx_queue, &msg, portMAX_DELAY))
        {

            ESP_LOGI("TX_WORKER", "Received request");

            // 2. Perform the slow blocking Web3 call
            // Since 'web3' is a global object, we can access it here.
            // Note: eth_call (reading) and sendEth (writing) can generally run concurrently
            // because they create separate HTTP clients.
            std::string txHash = web3.consumeNFT(msg.tokenID, msg.contractAddress);

            if (!txHash.empty())
            {
                ESP_LOGI("TX_WORKER", "Success! Tx Hash: %s", txHash.c_str());
            }
            else
            {
                ESP_LOGE("TX_WORKER", "Transaction Failed");
            }
        }
    }
}
static led_strip_handle_t led_strip;
void init_leds()
{
    led_strip_config_t strip_config = {};
    strip_config.strip_gpio_num = 17;
    strip_config.max_leds = 16;
    strip_config.color_component_format = LED_STRIP_COLOR_COMPONENT_FMT_GRB;
    strip_config.led_model = LED_MODEL_WS2812;

    led_strip_rmt_config_t rmt_config = {};
    rmt_config.resolution_hz = 10 * 1000 * 1000; // 10MHz
    rmt_config.clk_src = RMT_CLK_SRC_DEFAULT;
    rmt_config.mem_block_symbols = 64;
    rmt_config.flags.with_dma = false;

    ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_config, &rmt_config, &led_strip));
    led_strip_clear(led_strip);
}

void breathing_task(void *arg)
{
    float phase = 0;

    while (1)
    {
        switch (current_led_state)
        {
        case LED_STATE_WIFI_DISCONNECTED:
        {
            // 🔴 Breathing Red
            uint8_t brightness = (sin(phase) + 1.0f) * 127.5f;
            for (int i = 0; i < 16; i++)
                led_strip_set_pixel(led_strip, i, brightness, 0, 0);
            led_strip_refresh(led_strip);

            phase += 0.04f;
            if (phase > 2 * M_PI)
                phase = 0;
            vTaskDelay(pdMS_TO_TICKS(10));
            break;
        }

        case LED_STATE_WIFI_CONNECTED:
        {
            // 🟢 Solid Green
            for (int i = 0; i < 16; i++)
                led_strip_set_pixel(led_strip, i, 0, 50, 0);
            led_strip_refresh(led_strip);
            vTaskDelay(pdMS_TO_TICKS(100));
            break;
        }

        case LED_STATE_WEB3_CHECKING:
        {
            static float phase_yellow = 0;

            // Faster breathing than red
            uint8_t brightness =
                (sin(phase_yellow) + 1.0f) * 127.5f;

            for (int i = 0; i < 16; i++)
                led_strip_set_pixel(
                    led_strip,
                    i,
                    brightness * 0.8f,  // red dominant
                    brightness * 0.25f, // small green // green (slightly less for warm yellow)
                    0                   // blue
                );

            led_strip_refresh(led_strip);

            phase_yellow += 0.08f; // faster than red (which was ~0.04)
            if (phase_yellow > 2 * M_PI)
                phase_yellow = 0;

            vTaskDelay(pdMS_TO_TICKS(10));
            break;
        }

        case LED_STATE_WEB3_SUCCESS:
        {
            // Rapid green blink twice
            for (int k = 0; k < 2; k++)
            {
                for (int i = 0; i < 16; i++)
                    led_strip_set_pixel(led_strip, i, 0, 100, 0);
                led_strip_refresh(led_strip);
                vTaskDelay(pdMS_TO_TICKS(100));

                led_strip_clear(led_strip);
                led_strip_refresh(led_strip);
                vTaskDelay(pdMS_TO_TICKS(100));
            }

            current_led_state = LED_STATE_WIFI_CONNECTED;
            break;
        }

        case LED_STATE_WEB3_FAILED:
        {
            // Rapid red blink twice
            for (int k = 0; k < 2; k++)
            {
                for (int i = 0; i < 16; i++)
                    led_strip_set_pixel(led_strip, i, 100, 0, 0);
                led_strip_refresh(led_strip);
                vTaskDelay(pdMS_TO_TICKS(100));

                led_strip_clear(led_strip);
                led_strip_refresh(led_strip);
                vTaskDelay(pdMS_TO_TICKS(100));
            }

            current_led_state = LED_STATE_WIFI_CONNECTED;
            break;
        }
        }
    }
}

extern "C" void app_main(void)
{

    i2c_init();
    u8g2_Setup_sh1106_i2c_128x64_noname_f(
        &u8g2,
        U8G2_R0,
        u8x8_byte_esp32_hw_i2c,
        u8x8_gpio_and_delay_esp32);

    u8g2_InitDisplay(&u8g2);
    u8g2_SetPowerSave(&u8g2, 0);

    display_queue = xQueueCreate(5, sizeof(display_msg_t));
    xTaskCreate(display_task, "display", 4096, NULL, 5, NULL);

    wifi_event_group = xEventGroupCreate();
    init_leds();
    xTaskCreate(
        breathing_task,
        "breathing_task",
        4096,
        NULL,
        5,
        NULL);
    // 2. Un-mute ONLY your specific tags
    esp_log_level_set("MAIN", ESP_LOG_INFO);
    esp_log_level_set("WIFI", ESP_LOG_INFO); // Your wifi_event_handler logs
    esp_log_level_set("WEB3", ESP_LOG_INFO);
    esp_log_level_set("KEY", ESP_LOG_INFO);
    esp_log_level_set("TX_WORKER", ESP_LOG_INFO);
    esp_log_level_set("PN532", ESP_LOG_INFO);

    // // // }

    wifi_init();

    display_text("Connecting...", 12);
    // // // // xTaskCreatePinnedToCore(&wifi_init, "wifi_task", 4096, NULL, 5, NULL, 1);
    // xTaskCreatePinnedToCore(&retrieve_private_key, "private_key_task", 4096, NULL, 6, NULL, 1);

    xEventGroupWaitBits(wifi_event_group,
                        WIFI_CONNECTED_BIT,
                        pdFALSE,
                        pdTRUE,
                        portMAX_DELAY);

    display_text("Connected", 12);
    ESP_LOGI("MAIN", "WiFi is connected");

    tx_queue = xQueueCreate(5, sizeof(tx_msg_t));
    xTaskCreatePinnedToCore(
        tx_worker_task, // Function
        "tx_worker",    // Name
        8192,           // Stack size (Web3 needs big stack for SSL & JSON)
        NULL,           // Params
        1,              // Priority (Low priority is usually fine)
        NULL,           // Handle
        0               // <-- Run on CORE 0
    );
    const char *cardid = "";

    ESP_ERROR_CHECK(
        pn532_new_driver_spi(
            SPI_MISO, SPI_MOSI, SPI_SCK, SPI_CS, RESET_PIN, IRQ_PIN, SPI_HOST_NFC, SPI_CLOCKRATE, &pn532io));

    ESP_ERROR_CHECK(pn532_init(&pn532io));

    uint32_t version_data = 0;

    esp_err_t err = pn532_get_firmware_version(&pn532io, &version_data);

    if (err != ESP_OK)
    {
        ESP_LOGE("PN532", "Failed to get firmware version");
    }
    else
    {
        ESP_LOGI("PN532", "Found chip PN5%x",
                 (unsigned int)((version_data >> 24) & 0xFF));
    }

    ESP_ERROR_CHECK(pn532_SAM_config(&pn532io));

    ESP_LOGI("PN532", "Waiting for card...");

    while (1)
    {
        uint8_t uid[7] = {0};
        uint8_t uid_length = 0;

        esp_err_t err = pn532_read_passive_target_id(
            &pn532io,
            PN532_BRTY_ISO14443A_106KBPS,
            uid,
            &uid_length,
            1000 // timeout in ms
        );

        if (err == ESP_OK)
        {
            ESP_LOGI("PN532", "Card detected!");
            ESP_LOGI("PN532", "UID Length: %d", uid_length);
            ESP_LOG_BUFFER_HEX("PN532", uid, uid_length);

            char uid_str[32]; // enough for up to 16-byte UID

            uid_to_string(uid, uid_length, uid_str);

            ESP_LOGI("PN532", "UID String: %s", uid_str);

            xTaskCreatePinnedToCore(
                web3_task,   // task function
                "web3_task", // name
                12288,       // stack size (important)
                uid_str,     // params
                2,           // priority
                nullptr,     // task handle
                1            // 👈 CORE 1 (APP CPU)
            );

            vTaskDelay(pdMS_TO_TICKS(1000)); // avoid multiple rapid reads
        }

        //     vTaskDelay(pdMS_TO_TICKS(200));
        // // ✅ 1. CREATE QUEUE FIRST
        // key_queue = xQueueCreate(
        //     2,                    // queue length
        //     sizeof(privkey_msg_t) // item size
        // );

        // // assert(key_queue != NULL);
        // ESP_LOGI("QUEUE", "key_queue created: %p", key_queue);

        // xTaskCreatePinnedToCore(
        //     private_key_storage_task,
        //     "key_store",
        //     4096,
        //     NULL,
        //     5,
        //     NULL,
        //     0 // core 1
        // );
        // ESP_LOGI("MAIN", "Starting the 5 seconds simulation");
        // vTaskDelay(pdMS_TO_TICKS(5000));

        // ESP_LOGI("MAIN", "Simulation started....");
        // std::string myAddress = web3.getAddress();
        // web3.sendEth(myAddress, 0);
        // const char *hex_key = "eb23e1cb4055564bfbe03afa2b3ec0c037ee8624309d80ed429ad663c4612616";

        // hex_to_bytes_32(hex_key, keys.key);
        // xQueueSend(key_queue, &keys, portMAX_DELAY);
    }
}