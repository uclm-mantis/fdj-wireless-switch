// GPIO con interrupciones debe hacerse así:
// https://github.com/espressif/arduino-esp32/blob/master/libraries/ESP32/examples/GPIO/FunctionalInterruptStruct/FunctionalInterruptStruct.ino

#include <Arduino.h>

#include <esp_now.h>
#include <WiFi.h>

const char* TAG = "emisor";

typedef struct pulsador_ pulsador;
struct pulsador_ {
    const uint8_t pin;
    const uint8_t id;
    volatile bool presionado;
    volatile bool liberado;
    int lastV;
    TickType_t lastT;
};

pulsador p[] = {
    // pin id  press release v  t
    {  44, 1, false, false, 0, 0 },
    {  7,  2, false, false, 0, 0 },
    {  8,  3, false, false, 0, 0 },
    {  9,  4, false, false, 0, 0 }
};

const uint8_t N_PULSADORES = sizeof(p)/sizeof(p[0]);

// REPLACE WITH YOUR RECEIVER MAC Address
//EC:DA:3B:3A:69:78
//C0:4E:30:82:67:6C
//40:4C:CA:F9:E6:34
esp_now_peer_info_t peer = {
    .peer_addr = { 0x40, 0x4C, 0xCA, 0xF9, 0xE6, 0x34 },
    .channel = 0,
    .encrypt = false
};

typedef struct message_ {
  uint8_t id; // Identificador único para cada emisor
  bool presionado;
} message;


void ARDUINO_ISR_ATTR isr(void* param) {
    pulsador* button = (pulsador*) param;
    TickType_t current = xTaskGetTickCountFromISR();
    int v = digitalRead(button->pin);
    if ( v != button->lastV && current - button->lastT > 50 / portTICK_PERIOD_MS) { // debouncing
        button->presionado = (v != 0);
        button->liberado = (v == 0);
        button->lastV = v;
        button->lastT = current;
    }
}

void checkEvent(pulsador* button) {
    if (!button->presionado && !button->liberado) return;

    message msg = { button->id, button->presionado };
    ESP_ERROR_CHECK_WITHOUT_ABORT(esp_now_send(peer.peer_addr, (uint8_t *)&msg, sizeof(msg)));
    button->presionado = false; button->liberado = false;
}

void onDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  ESP_LOGI(TAG, "Delivery %s", (status == ESP_NOW_SEND_SUCCESS ? "Success" : "Fail"));
}

void setup() {
    Serial.begin(115200);
    WiFi.mode(WIFI_STA);
    ESP_ERROR_CHECK(esp_now_init());
    ESP_ERROR_CHECK(esp_now_register_send_cb(onDataSent));
    ESP_ERROR_CHECK(esp_now_add_peer(&peer));

    for (int i=0; i < N_PULSADORES; ++i) {
        uint8_t pin = p[i].pin;
        pinMode(pin, INPUT_PULLUP);
        p[i].lastV = digitalRead(pin);
        p[i].lastT = xTaskGetTickCount();
        attachInterruptArg(pin, isr, &p[i], CHANGE);
    }
}

void loop() {
    for (int i=0; i < N_PULSADORES; ++i) {
        checkEvent(&p[i]);
    }
    // Ahora debería dormir todo lo que pueda
    // https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/system/sleep_modes.html
}
