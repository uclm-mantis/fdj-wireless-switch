#include <freertos/FreeRTOS.h>
#include <nvs_flash.h>
#include <esp_event.h>
#include <esp_netif.h>
#include <esp_wifi.h>
#include <esp_now.h>
#include <esp_log.h>
#include <driver/gpio.h>

#if CONFIG_ESPNOW_WIFI_MODE_STATION
#define ESPNOW_WIFI_MODE WIFI_MODE_STA
#define ESPNOW_WIFI_IF   WIFI_IF_STA
#else
#define ESPNOW_WIFI_MODE WIFI_MODE_AP
#define ESPNOW_WIFI_IF   WIFI_IF_AP
#endif

const char* TAG = "emisor";

typedef struct pulsador_ pulsador;
struct pulsador_ {
    const gpio_num_t pin;
    const uint8_t id;
    volatile bool presionado;
    volatile bool liberado;
    int lastV;
    TickType_t lastT;
};

pulsador pulsadores[] = {
    {  .pin = GPIO_NUM_1, .id = 1 },
    {  .pin = GPIO_NUM_2, .id = 2 },
    {  .pin = GPIO_NUM_3, .id = 3 },
    {  .pin = GPIO_NUM_4, .id = 4 },
};

const uint8_t N_PULSADORES = sizeof(pulsadores)/sizeof(pulsadores[0]);

// TODO: peer_addr should not be hardcoded
esp_now_peer_info_t peer = {
    .peer_addr = { 0x40, 0x4c, 0xca, 0xf5, 0x24, 0xa8 },
    .channel = CONFIG_ESPNOW_CHANNEL,
    .ifidx = ESPNOW_WIFI_IF,
    .encrypt = false
};

typedef struct message_ {
  uint8_t id;
  bool presionado;
} message;

void register_button_event(pulsador* button) {
    int v = gpio_get_level(button->pin);
    if (v != button->lastV) { 
        button->presionado = (v == 0);
        button->liberado = (v != 0);
        button->lastV = v;
    }
}

void isr(void* param) {
    pulsador* button = (pulsador*) param;
    TickType_t current = xTaskGetTickCountFromISR();
    if (current - button->lastT > 50 / portTICK_PERIOD_MS) { // debouncing
        register_button_event(button);
        button->lastT = current;
    }
}

static void check_button_event(pulsador* button) {
    if (!button->presionado && !button->liberado) return;

    ESP_LOGI(TAG, "event %d %s", button->id, button->presionado?"PRESSED":"RELEASED");
    message msg = { button->id, button->presionado };
    ESP_ERROR_CHECK_WITHOUT_ABORT(esp_now_send(peer.peer_addr, (uint8_t *)&msg, sizeof(msg)));
    button->presionado = false; button->liberado = false;
}

static void entradas_init() {

    for (pulsador* p = pulsadores; p < pulsadores + N_PULSADORES; ++p) {
        gpio_reset_pin(p->pin);
        gpio_set_direction(p->pin, GPIO_MODE_INPUT);
        gpio_pullup_en(p->pin);
        gpio_set_intr_type(p->pin, GPIO_INTR_ANYEDGE);
        p->lastV = !gpio_get_level(p->pin);
        p->lastT = xTaskGetTickCount();
        gpio_isr_handler_add(p->pin, isr, p);
    }
}

static void wifi_init()
{
    // initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_set_mode(ESPNOW_WIFI_MODE));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_ERROR_CHECK(esp_wifi_set_channel(CONFIG_ESPNOW_CHANNEL, WIFI_SECOND_CHAN_NONE));

#if CONFIG_ESPNOW_ENABLE_LONG_RANGE
    ESP_ERROR_CHECK( esp_wifi_set_protocol(ESPNOW_WIFI_IF, WIFI_PROTOCOL_11B|WIFI_PROTOCOL_11G|WIFI_PROTOCOL_11N|WIFI_PROTOCOL_LR) );
#endif
}

static void espnow_send_cb(const uint8_t *mac, esp_now_send_status_t status)
{
    if (status != ESP_NOW_SEND_SUCCESS)
        ESP_LOGI(TAG, "Delivery failed %02x:%02x:%02x:%02x:%02x:%02x", mac[0],mac[1],mac[2],mac[3],mac[4],mac[5]);
}

static void espnow_init()
{
    ESP_ERROR_CHECK(esp_now_init());
    ESP_ERROR_CHECK(esp_now_register_send_cb(espnow_send_cb));
#if CONFIG_ESPNOW_ENABLE_POWER_SAVE
    ESP_ERROR_CHECK(esp_now_set_wake_window(CONFIG_ESPNOW_WAKE_WINDOW));
    ESP_ERROR_CHECK(esp_wifi_connectionless_module_set_wake_interval(CONFIG_ESPNOW_WAKE_INTERVAL));
#endif
    /* Set primary master key. */
    ESP_ERROR_CHECK(esp_now_set_pmk((uint8_t *)CONFIG_ESPNOW_PMK));
    ESP_ERROR_CHECK(esp_now_add_peer(&peer));
}

extern "C" void app_main(void)
{
    entradas_init();
    wifi_init();
    espnow_init();
    TickType_t last = xTaskGetTickCount();
    for(;;) {
        for (pulsador* p = pulsadores; p < pulsadores + N_PULSADORES; ++p) {
            check_button_event(p);
        }
        xTaskDelayUntil(&last, 50/ portTICK_PERIOD_MS);
        for (pulsador* p = pulsadores; p < pulsadores + N_PULSADORES; ++p) {
            register_button_event(p);
        }
        // go to sleep
    }
}
