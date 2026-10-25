# Haiku Radeon Legacy Driver

Driver para GPUs AMD Radeon legacy en Haiku. Arquitectura: GPU en userland,
helper de kernel mínimo, shim DRM compatible con la ABI de Mesa, verificador
de command streams y ejecución híbrida GPU+CPU.

Estado: **host-verificado, no ejecutado aún en Haiku ni en hardware real.**

## Estructura

```
src/
├── common/     tipos y protocolo IPC (C/C++)
├── helper/     radeon_hd.c (kernel helper, con backend simulado en host)
├── server/     radeon_gpud: memoria, GART, verificador, anillos, firmware,
│               ASIC DB, GPUVM, bloques IP, backend legacy
├── shim/       radeon + amdgpu (drmCommandWriteRead/mmap -> IPC)
├── asm/        kernels K1-K13 (referencia C + NEON x86-64)
├── lvp_hw/     planificador híbrido y oracle CPU
tests/          test_main.cpp (servidor/verificador/shims/GCN)
scripts/        gen_asic_db.py (ASIC DB + cabecera PCI)
docs/           PROGRESS, DECISIONS, REGISTER_OWNERSHIP
*/Jamfile       integración con el árbol de Haiku (helper/servidor/shim/asm)
```

## Compilar y probar (host)

```bash
make            # servidor, shims, helper y binarios de test
make test       # rg_asm_check + rg_test + radeon_gpud --selftest
make test-quick # idem, salida resumida
make test-asan  # suite con AddressSanitizer/UBSan
make asm-test   # solo K1-K13
make lint-fast  # -fsyntax-only sobre todo el código
make asic-db    # regenera rg_asic_db.inc + src/common/rg_pci_ids.h
```

Integración Haiku (desde el árbol de Haiku, con `RG_REPO` apuntando aquí):

```bash
jam -sRG_REPO=/ruta/AMD-DriverRadeonLegacy radeon_hd radeon_gpud
jam -sRG_REPO=/ruta/AMD-DriverRadeonLegacy -sRG_STANDALONE=1
```

Salida esperada en el host (macOS arm64, clang 21):

```text
rg_asm_check: 12 passed, 0 failed
rg_test: 61 passed, 0 failed
radeon_gpud --selftest: 0 failure(s)
```

## Uso del servidor

```bash
./build/radeon_gpud --enum       # lista la base de datos ASIC
./build/radeon_gpud --selftest   # valida subsistemas en memoria
./build/radeon_gpud --serve      # bucle de servicio (IPC real en Haiku)
```

## Seguridad

- Verificador deny-by-default: un IB solo se envía al anillo tras validar
  registros, opcodes y relocs, sobre una copia del IB (anti-TOCTOU).
- Cros-team: BOs, GPUVM y VMID están aislados por `team`.
- Sin firmware de CP el arranque legacy falla cerrado (`RG_ENOENT`).

## Limitaciones actuales

1. El helper usa APIs de Haiku solo bajo `__HAIKU__`; el host simula.
2. El shim usa transporte inyectable; en Haiku debe ser un `BPort`.
3. Constantes de registro/opcode y layouts de detile marcados `[VERIFICAR]`.
4. Sin pruebas en hardware; no se debe asumir aceleración funcional.
5. La evicción VRAM→GTT y `GartTable` no programan hardware todavía
   (`[HUMANO]`); en host solo modelan y preservan contenido.

## Licencia

Pendiente de definir. No incluir código GPL de otros drivers.
