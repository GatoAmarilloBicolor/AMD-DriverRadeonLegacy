# Progress Log - Haiku Radeon Legacy

Estado **medido** en el host de desarrollo (macOS arm64, clang 21). No se ha
ejecutado todavía en Haiku ni sobre hardware Radeon.

## Verificación actual

```text
make            -> compila sin errores
rg_asm_check    -> 12 passed, 0 failed   (K1-K13, diferencial)
rg_test         -> 61 passed, 0 failed   (servidor/GART/verificador/shims/GCN)
radeon_gpud --selftest -> 0 failures
radeon_gpud --enum     -> 17 entradas ASIC
```

Reproducir con `make test`. `make test-asan` recompila la suite con
AddressSanitizer/UBSan.

## Fase 1 - Helper de kernel
- [x] `src/helper/radeon_hd.c` con despacho de ioctls (`GET_INFO`, `MAP_MMIO`,
      `MAP_VRAM`, `TRANSLATE_PHYS`, `IRQ_ATTACH`, `IRQ_DETACH`, `PCI_CFG`).
- [x] Backend simulado cuando `!__HAIKU__`; APIs de Haiku tras `#ifdef`.
- [x] Tabla PCI semilla de arranque.
- [x] APIs reales bajo `#ifdef __HAIKU__`: `map_physical_memory`,
      `install_io_interrupt_handler`, config PCI, detección de BARs.
- [x] Detección PCI real (`pci_info`), IDs en `rg_pci_ids.h` y selección
      MMIO/VRAM; escrituras PCI denegadas (`RG_EACCES`).
- [x] Jamfiles: `src/helper` (integrado como `radeon_hd` o aislado con
      `RG_STANDALONE=1`), `src/server`, `src/shim`, `src/asm`.
- [ ] [HUMANO] Compilar/enlazar el Jamfile dentro del árbol real de Haiku y
      validar MMIO/IRQ en GPU física.
- [ ] [VERIFICAR] `get_memory_map` asume que `physical_entry.address` es el
      primer byte solicitado; confirmar con el kernel Haiku.

## Fase 2 - Servidor userland y memoria
- [x] `RgServer` (`src/server/ipc_server.cpp`) con despacho de todos los `RG_*`,
      resolución de BOs por `team` y limpieza `drop_team`.
- [x] `MemoryManager`: first-fit VRAM/GTT con fusión de huecos, fallback
      VRAM→GTT→CPU, mapeo host, tiling set/get, busy/idle.
- [x] `RingManager`: 2 anillos, fences por writeback simulado, timeouts,
      `soft_reset`, estadísticas.
- [x] `FirmwareLoader`: manifiestos por familia y carga desde
      `RG_FIRMWARE_DIR` o `/boot/system/data/firmware/radeon`.
- [x] Evicción LRU Fase 8: BOs VRAM idle → GTT, contenido preservado y
      `gpu_addr` actualizado; pruebas de 32K→40K y fallback a GTT.
- [ ] [HUMANO] Sustituir asignación host por `clone_area` real, mapear VRAM y
      ejecutar la copia de evicción con DMA/CP.
- [ ] [HUMANO] Conectar `GartTable` a `MemoryManager`/GPUVM y programar la
      tabla real (TLB, apertura GART).

## Fase 3 - Verificador de CS
- [x] `CsVerifier` deny-by-default, copia anti-TOCTOU, PM4 tipo 0/2/3,
      reglas registro FREE/RELOC/FORBID, validación y parcheo de relocs.
- [x] Prueba de fuzz (200k IBs) con invariante: aceptado ⇒ todo acceso cae
      dentro de un BO del cliente.
- [x] `rg_policy_deny_all` como default del servidor.
- [ ] Política de producción: reemplazar números provisionales por los de
      `r600d.h` / `evergreend.h` / `r300_reg.h` ([VERIFICAR]).
- [ ] [HUMANO] Fuzzing continuo con ASan sobre IB reales de Mesa.

## Fase 4 - Shim DRM
- [x] `radeon_shim.c`: transporte inyectable, `drmCommandWriteRead`,
      `drmIoctl`, `drmMmap`, ioctls GEM + `RADEON_INFO`.
- [x] `amdgpu_shim.c`: nombre, dev_info (CU count), VA_OP, timelines.
- [x] Prueba end-to-end create/mmap/tiling/info/escritura.
- [ ] [HUMANO] Reemplazar el transporte de prueba por el `BPort` real.

## Pista A - Ensamblador
- [x] K1-K13 con referencia C y despacho por arquitectura
      (`src/asm/common/rg_asm_ref.c`).
- [x] aarch64: `rg_asm_swizzle_neon`, `rg_asm_copy_nt_neon`,
      `rg_asm_memset128_neon`, `rg_asm_swap_endian_neon`
      (`src/asm/arm/detile_neon.S`).
- [x] x86-64: `rg_asm_swizzle_avx2`, `rg_asm_copy_nt_avx2`,
      `rg_asm_memset128_avx2`, `rg_asm_swap_endian_avx2`
      (`src/asm/x86/format_convert_sse2.S`), con despacho CPUID+XGETBV.
- [x] K8/K13 con colas no múltiplas; K1 valida n=0..69.
- [x] `rg_asm_check.c` con MT19937, roundtrip de detile y colas no múltiplas.
- [x] Convención fijada: TBL/swizzle procesa 16 B (4 px) en NEON y 32 B
      (8 px) en AVX2; los despachadores cuentan bytes, no píxeles.
- [ ] Sustituir el layout de bloques documentado por la fórmula exacta
      TeraScale/ADDR2 ([VERIFICAR]).

## Pista G - GCN/ASIC (parcial)
- [x] G0 `rg_asic`: 17 IDs R300→RDNA3, `rg_asic_lookup`, desconocido nunca
      acelera; `make asic-db` regenera la tabla.
- [x] G2 GPUVM por cliente (reserve/map/unmap/evict/move/translate/fault) y
      `VmidManager` con robo LRU; aislamiento mismo VA distinto PA probado.
- [x] G2 bloques IP con orden COMMON/NBIO→GMC→IH→PSP→SMU→GFX→SDMA,
      init/fini/suspend/resume/soft_reset.
- [x] G3 capa `amdgpu` (timelines + VA_OP) sobre el mismo servidor.
- [ ] G1 informe comparativo con RadeonGfx (pendiente).
- [ ] PSP/SMU reales, firmware GCN y modos de energía ([HUMANO]).

## Híbrido (§8)
- [x] `rg_planner_decide`: R600 compute→GPU con LDS; R300 geometry→CPU;
      cómputo con LDS en R600→CPU; oracle CPU (`lvp_exec_cpu`).
- [x] `rg_classify_compute` y `rg_cpu_compare` probados.

## Fase 8 - Evicción y GART
- [x] `MemoryManager` con reloj lógico, LRU, `try_evict_one` y
      `alloc_vram_evicting`; fallback y preservación de datos en host.
- [x] `GartTable` (`src/server/gart.{h,cpp}`): map/unmap/translate,
      páginas mapeadas y contador de flushes; test dedicado.
- [ ] [HUMANO] Copia VRAM→GTT por DMA/CP y flush de TLB/GART real.

## Pendiente crítico
1. Port real a Haiku (kernel + areas + BPort).
2. Constantes de registro/opcode desde fuentes AMD/Mesa (regla 3 del plan).
3. Firmware: verificar nombres y hashes; sin firmware el arranque legacy
   falla cerrado con `RG_ENOENT` (comportamiento intencional).
4. Hardware real y pruebas de estrés.
