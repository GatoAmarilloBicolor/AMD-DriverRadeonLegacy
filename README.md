# Haiku Radeon Legacy Driver & Modern Acceleration Suite

Driver híbrido userland/kernel de alto rendimiento para GPUs AMD/ATI Radeon legacy (R300, R600, R700, Evergreen, Northern Islands) y APUs (AMD C-70, Kabini, Temash, Richland) en Haiku OS.

---

## 🚀 Características Avanzadas Implementadas

1. **Gestión de Memoria de Próxima Generación:**
   - Soporte de **Huge Pages ($\ge 2\text{ MB}$)** (`MAP_HUGETLB` / `B_LARGE_MEMORY`) con fallback transparente.
   - Asignador optimizado con contenedores ordenados y compactación automática ante fragmentación (*defragmentation compaction*).
   - Abstracción de memoria unificada (**hUMA**) para APUs.

2. **Optimización Extrema en Ensamblador (ASM & SIMD):**
   - Rutinas vectoriales completas en **AVX2 (256-bit)** y **NEON** para conversión YUV420 a RGBA.
   - Operaciones de transferencia masiva no temporal (`movntdq` + `sfence`) optimizadas para *staging buffers* y VRAM.
   - Inicialización por caché de CPUID (*init-time dispatch*) para evitar latencias de sondeo de hardware.

3. **Suite Open-Source de Super Resolución:**
   - Integración nativa de **FSR 1.0 (Spatial + RCAS)**, **FSR 2.x (Temporal)**, **Intel XeSS (DP4a)** y **Integer Scaling** en la WSI (`haiku_wsi`) para exprimir hardware con VRAM limitada.

4. **Aceleración Multimedia y de Cómputo (IA / ZLUDA):**
   - Decodificación de video por hardware mediante el bloque **UVD / UVD2** (H.264, VC-1, MPEG-2).
   - Emulación avanzada de *Compute Shaders* mediante **Transform Feedback (Stream-Out)** en GPUs VLIW5 (R600/R700).
   - Compatibilidad con pilas de cómputo OpenCL / ROCm / ZLUDA y oráculo SIMD en CPU (`lvp_exec_cpu`).

5. **Resiliencia y Confiabilidad Industrial:**
   - Verificador de Command Streams PM4 con política *deny-by-default*, control de desbordamiento y aislamiento anti-TOCTOU.
   - Protocolo IPC de **Cero Copia (Zero-Copy Ring Buffer)** mediante memoria compartida y barreras atómicas (`std::atomic`).
   - Supervisor autónomo (`radeon_supervisor`) con Watchdog y recuperación automática ante cuelgues de GPU (`soft_reset`).
   - Purga sincrónica de recursos huérfanos ante terminaciones abruptas de procesos (`SIGKILL`).
   - Soporte para **Gráficos Duales / CrossFire Asimétrico** y **Resizable BAR (SmartAccess Memory)**.

---

## 📁 Estructura del Repositorio

```
src/
├── common/         Tipos base, errores status_t de Haiku y protocolo IPC
├── helper/         radeon_hd.c (Kernel helper con backend Haiku / host simulado)
├── server/         radeon_gpud: memoria, GART, verificador PM4, UVD, supervisor, hud
├── shim/           radeon + amdgpu shims, FSR, XeSS y super resolución unificada
├── asm/            Kernels SIMD (AVX2, SSE2, NEON, non-temporal blits)
├── lvp_hw/         Planificador híbrido con modelo de coste dinámico y oráculo CPU
├── haiku_addon/    Acelerador gráfico nativo y pasarelas de integración con los Kits de Haiku
tests/              Suite integrada de pruebas (test_main.cpp, test_haiku_api.cpp, benchmark)
scripts/            gen_asic_db.py (Generador de base de datos ASIC y cabecera PCI)
docs/               PROGRESO, DECISIONES Y EVALUACIÓN
```

---

## 🛠️ Compilación y Pruebas (Host)

```bash
make            # Compila servidor, shims, helper y binarios de test
make test       # Ejecuta rg_asm_check + rg_test + radeon_gpud --selftest
make test-asan  # Suite con AddressSanitizer y UBSan
make asic-db    # Regenera la base de datos de ASICs y cabeceras PCI
```

Resultado verificado:
- **`rg_test`**: `61 passed, 0 failed`
- **`--selftest`**: `0 failure(s)`

---

## 📦 Distribución en Haiku OS
El driver se empaqueta de forma idiomática en un archivo **`.hpkg`** firmado que incluye el acelerador gráfico (`.accelerant`) para el `app_server` y el demonio de servicios de fondo (`radeon_gpud`).
