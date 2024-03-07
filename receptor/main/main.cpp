#include <Arduino.h>
#include <esp_now.h>
#include <WiFi.h>

typedef struct salida_ salida;
struct salida_
{
    const uint8_t id;
    const uint8_t pin;
    const uint8_t pinModoPulsador;
};

salida salidas[] = {
    { .id = 1, .pin = 6,   .pinModoPulsador = 0 },
    { .id = 2, .pin = 7,   .pinModoPulsador = 1 },
    { .id = 3, .pin = 8,   .pinModoPulsador = 3 },
    { .id = 4, .pin = 10,  .pinModoPulsador = 4 },
};

const uint8_t N_SALIDAS = sizeof(salidas) / sizeof(salidas[0]);

typedef struct message_ {
  uint8_t id; // Identificador único para cada emisor
  bool presionado;
} message;

void onDataRecv(const esp_now_recv_info_t * esp_now_info, const uint8_t *data, int data_len)
{
    message* msg = (message*)data;

    for (salida* s = salidas; s < salidas + N_SALIDAS; ++s) {
        if (msg->id == s->id) {
            if (digitalRead(s->pinModoPulsador)) digitalWrite(s->pin, msg->presionado);
            else if (msg->presionado) digitalWrite(s->pin, !digitalRead(s->pin));
            break;
        }
    }
}

void setup()
{
    Serial.begin(115200);
    WiFi.mode(WIFI_STA);

    ESP_ERROR_CHECK(esp_now_init());
    ESP_ERROR_CHECK(esp_now_register_recv_cb(onDataRecv));
}

void loop()
{
}