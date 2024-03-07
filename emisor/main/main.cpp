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

pulsador pulsadores[] = {
    {  .pin = 1, .id = 1 },
    {  .pin = 2, .id = 2 },
    {  .pin = 3, .id = 3 },
    {  .pin = 4, .id = 4 },
};

const uint8_t N_PULSADORES = sizeof(pulsadores)/sizeof(pulsadores[0]);

// REPLACE WITH YOUR RECEIVER MAC Address
//EC:DA:3B:3A:69:78
//C0:4E:30:82:67:6C
//40:4C:CA:F9:E6:34
esp_now_peer_info_t peer = {
    .peer_addr = { 0x40, 0x4c, 0xca, 0xf5, 0x24, 0xa8 },
    .channel = 0,
    .encrypt = false
};

typedef struct message_ {
  uint8_t id;
  bool presionado;
} message;


void registerButtonEvent(pulsador* button) {
    int v = digitalRead(button->pin);
    if (v != button->lastV) { 
        button->presionado = (v != 0);
        button->liberado = (v == 0);
        button->lastV = v;
    }
}

void ARDUINO_ISR_ATTR isr(void* param) {
    pulsador* button = (pulsador*) param;
    TickType_t current = xTaskGetTickCountFromISR();
    if (current - button->lastT > 50 / portTICK_PERIOD_MS) { // debouncing
        registerButtonEvent(button);
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

    for (pulsador* p = pulsadores; p < pulsadores + N_PULSADORES; ++p) {
        pinMode(p->pin, INPUT_PULLUP);
        p->presionado = false; p->liberado = false;
        p->lastV = digitalRead(p->pin);
        p->lastT = xTaskGetTickCount();
        attachInterruptArg(p->pin, isr, p, CHANGE);
    }
}

void loop() {
    for (pulsador* p = pulsadores; p < pulsadores + N_PULSADORES; ++p) {
        checkEvent(p);
    }
    delay(100);
    for (pulsador* p = buttons; p < buttons + N_PULSADORES; ++p) {
        registerButtonEvent(p);
    }
    // Ahora debería dormir todo lo que pueda
    // https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/system/sleep_modes.html
}
