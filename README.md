# FdJ Wireless Switch

Primera versión del conmutador inalámbrico para juguetes adaptados. Utiliza ESP Now! sin cifrado y sin filtrado de fuente.

## Configuración del emisor

En `emisor/main/main.cpp` editar la definición de pulsadores. El campo `.id` se utiliza para ligarlo a la salida con el mismo valor en el receptor.

```c
pulsador p[] = {
    {  .pin = 44, .id = 1 },
    {  .pin = 7,  .id = 2 },
    {  .pin = 8,  .id = 3 },
    {  .pin = 9,  .id = 4 },
};
```

En el mismo archivo, justo a continuación, editar la definición de `peer`. Solo debería editarse la MAC en `.peer_addr` y el canal WiFi en `.channel`.

```c
esp_now_peer_info_t peer = {
    .peer_addr = { 0x40, 0x4C, 0xCA, 0xF9, 0xE6, 0x34 },
    .channel = 0,
    .encrypt = false
};
```

## Configuración del receptor

En `receptor/main/main.cpp` editar la definición de las salidas. El campo `.id` debe coincidir con el correspondiente emisor. El campo `.pinModoPulsador` es una entrada digital para configurar modo pulsador (*press* activa, *release* desactiva) o modo conmutador (*press* conmuta el estado).

```c
salida salidas[] = {
    { .id = 1, .pin = 6,   .pinModoPulsador = 0 },
    { .id = 2, .pin = 7,   .pinModoPulsador = 1 },
    { .id = 3, .pin = 8,   .pinModoPulsador = 3 },
    { .id = 4, .pin = 10,  .pinModoPulsador = 4 },
};
```

# TODO

- Mecanismo para configurar peer.
- Deep sleep al menos en emisor.
