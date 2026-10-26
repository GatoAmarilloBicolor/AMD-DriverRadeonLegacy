# Integración con Haiku

Estado: el contrato, el módulo de kernel y el acelerador existen y compilan
contra cabeceras reales de Haiku (`make haiku-check`). **No se han ejecutado
en Haiku ni sobre hardware real**: no hay toolchain ni GPU de destino en el
host de desarrollo (macOS arm64). Todo lo marcado `[VERIFICAR]` necesita
prueba en hierro.

## Arquitectura

```
app_server
   │  carga add-ons/accelerants/radeon_legacy.accelerant
   ▼
acelerador (userland)  ──ioctl(RG_IOCTL_*)──►  módulo de kernel
   src/haiku_addon/                             src/kernel/
                                                   │
                                                   ├─ PCI / BARs / VRAM / IRQ
                                                   └─ /dev/graphics/radeon_legacy_BDF
```

- El acelerador **no** enlaza el servidor ni `libradeon_gpud`; solo usa el
  contrato `RG_IOCTL_*` de `src/helper/helper_ioctls.h`.
- El módulo publica `/dev/graphics/radeon_legacy_<bus><dev><fn>` y responde a
  `B_GET_ACCELERANT_SIGNATURE` con `radeon_legacy.accelerant`.
- El servidor (`radeon_gpud`) sigue siendo el camino IPC para clientes
  (CUDA/ROCm simulado, PM4, memoria). Es independiente de este camino de
  display.

## Dos caminos posibles

### Camino A — reutilizar `radeon_hd` (recomendado a corto plazo)

Haiku ya trae `radeon_hd` con AtomBIOS, DDC/EDID, DPMS y page flip reales
para muchas de estas GPUs. Nuestro `src/helper/radeon_hd.c` implementa el
mismo contrato `RG_IOCTL_*` sobre ese helper. Ventaja: modosetting real sin
portar AtomBIOS. Desventaja: dependemos de que `radeon_hd` reconozca la GPU.

### Camino B — módulo autónomo (este repo)

`src/kernel/radeon_legacy_kernel.cpp` es un driver legacy completo en su
parte de recursos (PCI, BARs, VRAM, IRQ, config space, traducción de áreas).
El display queda deliberadamente en scaffold hasta portar AtomBIOS:

| Operación            | Estado                                             |
|----------------------|----------------------------------------------------|
| `GET_INFO`           | real (IDs, BARs, VRAM, IRQ, familia)               |
| `MAP_MMIO`/`MAP_VRAM`| real (`map_physical_memory`, flags por familia)     |
| `TRANSLATE_PHYS`     | real (offsets de área + `get_memory_map`)           |
| `IRQ_ATTACH/DETACH`  | real; ack write-1-to-clear `[VERIFICAR]`            |
| `PCI_CFG`            | real (1/2/4 bytes)                                  |
| `GET_CONNECTORS`     | scaffold: 1 conector, sin EDID                      |
| `GET_CRTCS`          | scaffold: 1 CRTC 1024x768                           |
| `SET_CRTC`           | `B_NOT_SUPPORTED` (falta backend AtomBIOS)          |
| `PAGE_FLIP`          | `B_NOT_SUPPORTED`                                   |
| `HOTPLUG_EVENT`      | `B_NOT_SUPPORTED`                                   |
| `BPORT_TRANSPORT`    | `B_NOT_SUPPORTED`                                   |

## Matriz de hooks del acelerador

Implementados: init/uninit/clone, clone-info, device-info, mode-count,
mode-list, propose/preferred/set/get mode, frame-buffer-config,
pixel-clock-limits, timing-constraints, EDID (si el kernel lo entrega),
DPMS capabilities/mode (sin set), engine-count (0) y sincronización trivial.

Devuelven `NULL` a propósito: cursor (sin memoria de cursor), blits/rellenos
2D (sin motor 2D), overlay y brillo. `SET_CRTC`/`SET_DPMS` devuelven
`B_NOT_SUPPORTED` hasta que exista el backend de modos.

Lista de modos expuesta (VESA 60 Hz, `B_RGB32`): 640x480, 800x600, 1024x768,
1280x1024, 1600x1200. `frame_buffer_dma` apunta a la VRAM física; el offset
sale de `h_display_start`/`v_display_start`.

## Compilación

Los `Jamfile` están pensados para enlazarse dentro del árbol de Haiku; `jam`
no incluye archivos fuera de `HAIKU_TOP`.

```bash
# En el árbol de Haiku:
ln -s "$RG_REPO/src/haiku_addon" \
      "$HAIKU/src/add-ons/accelerants/radeon_legacy"
ln -s "$RG_REPO/src/kernel" \
      "$HAIKU/src/add-ons/kernel/drivers/graphics/radeon_legacy"

cd "$HAIKU"
jam -sRG_REPO=/ruta/AMD-DriverRadeonLegacy radeon_legacy
jam -sRG_REPO=/ruta/AMD-DriverRadeonLegacy radeon_legacy.accelerant
```

Resultado esperado:

- `radeon_legacy` → `add-ons/kernel/drivers/graphics/`
- `radeon_legacy.accelerant` → `add-ons/accelerants/`

En el host macOS solo hay comprobación sintáctica:

```bash
make haiku-check   # -D__HAIKU__ contra src/kernel/haiku_stub
```

`src/kernel/haiku_stub/` contiene copias mínimas de las cabeceras públicas
(la de `Accelerant.h` es literal de Haiku, MIT). No se usa en Haiku real.

## Contrato ioctl

Definido en `src/helper/helper_ioctls.h`. Puntos críticos ya fijados:

- `rg_display_timing` = 24 bytes (`flags` en 20).
- `rg_display_mode` = 40 bytes (`flags` en 36), idéntico a `display_mode`.
- Polaridad en `flags` (`RG_TIMING_POSITIVE_HSYNC/VSYNC`), no campos aparte.
- Éxito IPC = bytes de respuesta (> 0); errores = `RG_*` negativos.

Cualquier cambio en esos structs debe actualizar `tests/test_haiku_api.cpp`
y los `static_assert` del acelerador.

## Constantes y propiedad de registros

- `RADEON_CONFIG_MEMSIZE = 0x00f8`, máscara `0x1ff00000` (R300–R500). Ojo:
  `0x0f50` es `CAPABILITIES_ID`, no tamaño de VRAM.
- `R600_CONFIG_MEMSIZE = 0x5428`: bytes en R600/R700, MB en Evergreen/NI
  discretas, bytes en APUs Palm/Sumo/Sumo2.
- MMIO: BAR2 hasta pre-Bonaire, BAR5 desde Bonaire (`device_id >= 0x9830`
  como heurística, con fallback al otro BAR).
- Límite de pixel clock 400 MHz `[VERIFICAR]`; IRQ ack write-1-to-clear
  `[VERIFICAR]`; temporizaciones son VESA estándar, no EDID.
- No añadir constantes inventadas: usar `r300_reg.h`/`r600d.h`/`evergreend.h`,
  Mesa (MIT) o PDFs de AMD, y marcar `[VERIFICAR]` hasta confirmarlas.
- Nunca copiar código GPL de otros drivers.

## Verificación en hardware (pendiente)

1. `lspci -nn` confirma `1002:<id>` y que el dispositivo está libre.
2. Cargar `radeon_legacy`; comprobar `/dev/graphics/radeon_legacy_*`.
3. `GET_INFO`: BARs distintos de 0, `vram_size` plausible, IRQ asignada.
4. `MAP_VRAM` + escritura/lectura de un patrón en el inicio de VRAM.
5. `IRQ_ATTACH`: provocar vblank/hotplug y ver el semáforo liberarse.
6. Preferencias de pantalla: debe listar los 5 modos; `SET_DISPLAY_MODE`
   seguirá fallando hasta el backend de modos.
7. `radeon_gpud --selftest` y `make test` en Haiku (sin regresiones).
8. Confirmar que `radeon_hd` no reclamó antes el mismo dispositivo.

## Siguiente trabajo

1. Backend de modos: AtomBIOS (o reutilizar el de `radeon_hd`) para
   `SET_CRTC`, DPMS y page flip.
2. DDC/EDID por I2C en `GET_CONNECTORS`; HPD real.
3. Memoria de cursor y motor 2D (o dejar software).
4. Sustituir el transporte de test del servidor por `BPort` y áreas
   compartidas para IB/relocs cuando se integre `radeon_gpud`.
5. Validar el build dentro del árbol real de Haiku y empaquetar.
