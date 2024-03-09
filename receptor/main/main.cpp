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
#define ESPNOW_WIFI_IF   ESP_IF_WIFI_STA
#else
#define ESPNOW_WIFI_MODE WIFI_MODE_AP
#define ESPNOW_WIFI_IF   ESP_IF_WIFI_AP
#endif

static const char *TAG = "receptor";

typedef struct salida_ salida;
struct salida_
{
    const uint8_t id;
    const gpio_num_t pin;
    const gpio_num_t pinModoPulsador;
    unsigned v;
};

salida salidas[] = {
    { .id = 1, .pin = GPIO_NUM_1,  .pinModoPulsador = GPIO_NUM_5 },
    { .id = 2, .pin = GPIO_NUM_2,  .pinModoPulsador = GPIO_NUM_6 },
    { .id = 3, .pin = GPIO_NUM_3,  .pinModoPulsador = GPIO_NUM_7 },
    { .id = 4, .pin = GPIO_NUM_4,  .pinModoPulsador = GPIO_NUM_8 },
};

const uint8_t N_SALIDAS = sizeof(salidas) / sizeof(salidas[0]);


static void salidas_init()
{
    for (salida* s = salidas; s < salidas + N_SALIDAS; ++s) {
        gpio_reset_pin(s->pin);
        gpio_set_direction(s->pin, GPIO_MODE_OUTPUT);
        
        gpio_reset_pin(s->pinModoPulsador);
        gpio_set_direction(s->pinModoPulsador, GPIO_MODE_INPUT);
        gpio_pullup_en(s->pinModoPulsador);
        gpio_set_level(s->pin, (s->v = 0));
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

typedef struct message_ {
  uint8_t id;
  bool presionado;
} message;

static void espnow_receiver_cb(const esp_now_recv_info_t * info, const uint8_t *data, int data_len)
{
    message* msg = (message*)data;

    // TODO: si recibe broadcast añade a la lista de peers (ver ejemplo de ESP-IDF)
    for (salida* s = salidas; s < salidas + N_SALIDAS; ++s) {
        if (msg->id == s->id) {
            if (gpio_get_level(s->pinModoPulsador)) gpio_set_level(s->pin, (s->v = msg->presionado));
            else if (msg->presionado) gpio_set_level(s->pin, (s->v = !s->v));
            ESP_LOGI(TAG, "Hit %d = %d (%s)", msg->id, s->v, msg->presionado?"PRESS":"RELEASE");
            break;
        }
    }
}

static void espnow_init()
{
    ESP_ERROR_CHECK(esp_now_init());
    ESP_ERROR_CHECK(esp_now_register_recv_cb(espnow_receiver_cb));
#if CONFIG_ESPNOW_ENABLE_POWER_SAVE
    ESP_ERROR_CHECK(esp_now_set_wake_window(CONFIG_ESPNOW_WAKE_WINDOW));
    ESP_ERROR_CHECK(esp_wifi_connectionless_module_set_wake_interval(CONFIG_ESPNOW_WAKE_INTERVAL));
#endif
    ESP_ERROR_CHECK(esp_now_set_pmk((uint8_t *)CONFIG_ESPNOW_PMK));
}

extern "C" void app_main(void)
{
    salidas_init();
    wifi_init();
    espnow_init();
}
