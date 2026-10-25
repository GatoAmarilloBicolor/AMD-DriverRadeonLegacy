# Evaluación del trabajo previo de X512 (RadeonGfx) — G1 / §7A.11

## 1. Contexto y Objetivos
El repositorio `X547/RadeonGfx` representa el esfuerzo pionero más completo en Haiku para conectar controladores de GPU AMD con la pila de userland mediante un servidor gráfico dedicado (`RadeonServer`), un driver auxiliar de kernel para abstracción MMIO/IRQ/traducción física, y un shim para libdrm.

En cumplimiento con la regla §0.10 y el paso G1 de `PLAN_RADEON_LEGACY_HAIKU-2.md`, este documento evalúa su arquitectura, determina los componentes reutilizables y registra la decisión de diseño adoptada.

---

## 2. Hallazgos Técnicos de RadeonGfx
1. **Modelo de Servidor y Kernel Helper:** Emplea el patrón de servidor en userland (`RadeonServer` con `ServerThreadLink`) y un helper de kernel que publica un dispositivo en `/dev/graphics` para mapear áreas físicas (`area_id`) y manejar interrupciones mediante semáforos. Este diseño validó que la arquitectura desacoplada evita caídas del kernel en Haiku.
2. **Limitaciones Arquitectónicas:**
   - **Licenciamiento:** Carece de un archivo de licencia explícito (derechos de autor por defecto restrictivos), lo que impide la adopción directa de su código fuente en un proyecto con vocación upstream MIT.
   - **Dependencias Externas:** Requiere subproyectos acoplados no estandarizados (`ThreadLink`, `SADomains`, `Locks`).
   - **Alcance de Hardware:** Está orientado casi exclusivamente a familias GCN/amdgpu modernas (GFX6+), dejando fuera el soporte unificado para GPUs legacy TeraScale (R300–R700, Evergreen, Northern Islands), el cual requiere un verificador de command streams (`CsVerifier`) estricto con tablas de registros.

---

## 3. Decisión Registrada (ADR-G1)
**Estrategia adoptada:** *Reimplementación en la arquitectura unificada y limpia de `radeon_gpud` (MIT compatible).*

- **Qué se reutiliza (conceptual):** El patrón de comunicación IPC por áreas compartidas y semáforos de Haiku, la separación entre modesetting de kernel (`radeon_hd`) y renderizado en userland, y el esquema de ioctls para MMIO/VRAM.
- **Qué se desecha:** El monolito de código acoplado y las dependencias propietarias de sincronización.
- **Ventaja:** Un codebase limpio en C++17 modular, con licencia MIT clara, que soporta tanto el espectro legacy (R300-NI) con verificador CS como el espectro GCN/amdgpu con GPUVM y Vulkan 1.4, optimizado para Haiku x86 y ARM.
