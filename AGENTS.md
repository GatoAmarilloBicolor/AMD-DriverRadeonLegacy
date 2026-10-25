# AGENTS.md - Haiku Radeon Legacy Driver

## Contexto
Driver para Haiku sobre GPUs Radeon legacy y GCN tempranas. El host de
desarrollo es macOS arm64; el objetivo real es Haiku (x86-64 y arm64).
Todo el código de plataforma debe quedar tras `#ifdef __HAIKU__` y el host
compila un backend simulado.

## Comandos verificados

```bash
make              # todo: servidor, shims, helper, tests
make userland     # libradeon_gpud.a + radeon_gpud
make helper       # radeon_hd.o (simulado en host)
make shim         # solo shims
make test         # rg_asm_check + rg_test + --selftest
make test-quick   # tests con salida resumida
make test-asan    # AddressSanitizer + UBSan
make asm-test     # solo kernels K1-K13
make lint-fast    # -fsyntax-only en todo el código
make typecheck    # alias de lint-fast
make docs         # valida docs + regenera ASIC DB
make asic-db      # regenera rg_asic_db.inc
make clean
```

Resultado esperado hoy: `rg_asm_check 12/12`, `rg_test 61/61`,
`--selftest 0 failures`.

## Mapa de código

- `src/common/rg_types.h`, `rg_ipc.h`: tipos, errores `RG_*`, mensajes IPC.
  Sin dependencias de Haiku; se comparten entre C y C++.
- `src/helper/helper_ioctls.h`, `radeon_hd.c`: contrato del helper de kernel.
  Ioctls: `GET_INFO`, `MAP_MMIO`, `MAP_VRAM`, `TRANSLATE_PHYS`, `IRQ_ATTACH`,
  `IRQ_DETACH`, `PCI_CFG`. En host: backend simulado.
- `src/server/memory_manager.*`: BOs VRAM/GTT/CPU, first-fit con fusión,
  fallback VRAM→GTT→CPU, tiling, busy/idle.
- `src/server/cs_verifier.*`: verificador PM4 deny-by-default. Política de
  producción pendiente de constantes reales.
- `src/server/ring_manager.*`: anillos GFX/CP_DMA, fences, soft reset.
- `src/server/firmware.*`: manifiestos por familia; fallo cerrado.
- `src/server/asic/*`: base de datos y lookup; `rg_asic_unknown` no acelera.
- `src/server/gcn/gpuvm.*`: VA por cliente y VMID LRU.
- `src/server/gcn/ip_blocks.*`: orden de arranque de bloques IP.
- `src/server/legacy/rg_legacy.*`: secuencia de arranque y ownership §6.7.
- `src/server/ipc_server.*`, `main.cpp`: despacho por `team` y CLI.
- `src/shim/radeon_shim.*`, `amdgpu_shim.*`: transporte inyectable.
- `src/asm/common/*`: referencias C, despacho y `rg_asm_check.c`.
- `src/asm/arm/detile_neon.S`, `src/asm/x86/format_convert_sse2.S`.
- `src/lvp_hw/*`: planificador híbrido y oracle CPU.
- `tests/test_main.cpp`: suite integrada.

## Convenciones críticas

1. **Éxito IPC = bytes de respuesta (> 0)**. Errores: `RG_*` negativos.
   Inicializar siempre el `status` del payload.
2. **PM4**: bits 16..29 = total de dwords − 1, incluye cabecera. Tipo 0/3
   consumen `total`; `SET_*_REG` tiene `(total-1)/2` pares.
3. **ASM**: NEON procesa 16 B (4 px) por bloque; AVX2 32 B (8 px). Contar
   bytes/píxeles, no "bloques de 16/32 píxeles".
4. **Verificador**: nunca enviar al anillo un IB que haya fallado; se valida
   y parchea una copia. Los dwords de dirección son índices de reloc.
5. **Memoria**: todo BO pertenece a un `team`; cualquier acceso cruzado es
   `RG_EACCES`. Liberar en `drop_team`.
6. **Firmware**: si falta el microcódigo de CP, abortar (`RG_ENOENT`).
7. **ASIC**: ID desconocido ⇒ `RG_FAMILY_UNKNOWN` ⇒ CPU.
8. No añadir constantes de registro "inventadas": deben venir de
   `r300_reg.h`/`r600d.h`/`evergreend.h`, Mesa o PDFs de AMD. Marcar
   `[VERIFICAR]` mientras no se confirme.
9. Nunca copiar código GPL de otros drivers.

## Trampas conocidas

- `make` sin `-MMD` deja objetos obsoletos al cambiar cabeceras; el Makefile
  ya genera dependencias, pero `make clean` si algo huele mal.
- `rg_asic.h` expone símbolos con `extern "C"` para el shim en C.
- `ring_manager.h` guarda stats por anillo; no referenciar campos inexistentes.
- `rg_legacy.cpp` usa punteros a método (`(this->*fn)()`), no funciones libres.
- `CsVerifier` guarda un puntero a la política para permitir reasignación.

## Port a Haiku (siguiente fase)

1. Helper: `map_physical_memory`, `clone_area`, `install_io_interrupt_handler`,
   `create_area` para VRAM/GTT; publicar dispositivo en `/dev/graphics`.
2. Servidor: sustituir transporte de test por `BPort`; áreas compartidas para
   IB y relocs.
3. ASM: el Makefile ya compila `.S` en host; en Haiku usar las mismas fuentes
   con `-Wa,--noexecstack` y verificar ABI.
4. Jamfiles en `src/{helper,server,shim,asm}`; validar el build dentro del
   árbol real de Haiku y empaquetar.
