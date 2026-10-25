# Register Ownership - radeon_hd vs radeon_gpud

Documentación de la separación de registros entre el driver de kernel (modesetting) y el servidor en userland (render y gestión).

## 1. radeon_hd / accelerant (Kernel / Modesetting)
- **CRTC:** Controles de pantalla, generadores de timing, scanout.
- **PLL de pantalla:** Relojes de píxeles y convertidores DVO/HDMI/DP.
- **AtomBIOS:** Inicialización de tablas de BIOS, modesetting y DDC/conectores.
- **Cursor:** Posicionamiento y bitmaps del cursor de hardware.

## 2. radeon_gpud (Servidor Userland)
- **CP (Command Processor):** Anillos de comandos PFP, ME, RLC.
- **GART / GPUVM:** Tablas de páginas y control de memoria virtual.
- **Memory Controller (MC):** Solo lectura de límites de VRAM y apertures.
- **IH (Interrupt Handler):** Recepción y enmascaramiento de interrupciones de GPU.
- **Pipeline 3D / 2D:** Bloques de sombreado, rasterización y sincronización.
