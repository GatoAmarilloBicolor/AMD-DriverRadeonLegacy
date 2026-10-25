Plan de implementación: driver nativo Radeon legado para Haiku
r300–r500 · r600/r700 · Evergreen/Northern Islands — OpenGL por Mesa/Gallium y Vulkan 1.0 híbrido GPU+CPU
Documento pensado para que lo ejecute un agente de desarrollo. Versión 4 — 2026-10-24. Cambios respecto a v3: soporte de todo el espectro amdgpu (GCN, Vega, RDNA; §7A), enumeración y capa ASIC unificada, GPUVM por cliente y pista G (§9.3). Cambios de v2 a v3: cómputo emulado por la tubería gráfica en R600/R700 (§8.14), límites realistas del ensamblador para cómputo (§8A.7), kernels K12/K13 y fase 10a. Cambios de v1 a v2: ejecutor híbrido GPU+CPU para Vulkan (§8), estrategia de ensamblador x86/ARM (§8A), consideraciones de plataforma (§6.10) y nuevas fases (§9). Marcas: ✔ = comprobado al redactar (con fuente en §12). [VERIFICAR] = leer la fuente primaria antes de depender de ello.


________________


0. Reglas para el agente
1. Ejecuta las fases de §9 en orden. Cada fase tiene criterios de aceptación (CA). No avances si un CA falla; registra el motivo y el log en docs/PROGRESS.md.
2. Los pasos marcados [HUMANO] necesitan hardware real. El agente prepara binarios, script y checklist, y pide el log; no simula resultados.
3. Ninguna constante de registro se inventa. Salen de r600d.h, evergreend.h, r300_reg.h (Mesa, MIT) o de los PDF de AMD (§12).
4. Licencias. Haiku y Mesa son MIT. Del driver radeon de Linux, cada archivo trae su cabecera: comprueba archivo por archivo antes de reutilizar nada. Si hay duda, reimplementa desde la documentación de AMD y usa Linux solo para entender el orden de inicialización. Nunca pegues código GPL.
5. Seguridad primero. Estas GPUs no tienen memoria virtual por proceso. Un cliente que envíe comandos sin verificar puede leer o escribir toda la RAM (§6.4). El verificador es obligatorio antes de cualquier publicación.
6. Commits pequeños (un componente por commit). Primero corrección, luego rendimiento.
7. Si una decisión de este documento choca con lo que ves en el código real, detente, documenta la discrepancia en docs/DECISIONS.md y propón un cambio; no improvises en silencio.
8. Ensamblador (§8A): todo kernel en ensamblador nace con su referencia en C, despacho en tiempo de ejecución y prueba de equivalencia. Sin las tres cosas, no se integra.
9. Oráculo CPU: cualquier resultado de la GPU en Vulkan se valida contra el camino CPU (RG_VK_MODE=cpu-only) antes de darlo por bueno.
10. No duplicar trabajo existente: antes de escribir el backend de familias amdgpu, evalúa el trabajo previo de X512 (RadeonGfx, libdrm2) y registra la decisión adoptar / forkear / reimplementar (§7A.11).


________________


1. Estado del arte y decisión de arquitectura
1.1 Lo que ya existe en Haiku
Hecho
	Estado
	Vulkan Loader y Lavapipe (Vulkan por CPU) funcionan en Haiku ✔
	Foro Haiku, hilo "Vulkan lavapipe software rendering is working on Haiku"
	RADV (Vulkan, AMD GCN+) corre en Haiku ✔
	Lo logró el desarrollador X512 con un libdrm propio que habla por IPC con un servidor GPU en userland ("RadeonGfx"), con MMIO por area_id, IRQ por sem_id y un driver auxiliar para traducir direcciones físicas
	Zink (OpenGL sobre Vulkan) sobre RADV funciona, también con varios procesos ✔
	Phoronix, nov. 2021
	Haiku no tiene DRM/GEM/TTM ✔
	Ticket Haiku #9919
	radeon_hd hace modesetting para R600+ sin aceleración
	[VERIFICAR] en src/add-ons/kernel/drivers/graphics/radeon_hd
	No hay Vulkan para GPUs antiguas ✔
	Comentarios en el foro; RADV empieza en GCN
	1.2 Decisiones de arquitectura (ADR)
* ADR-001 — Servidor GPU en userland, con un driver de kernel mínimo (helper). Se reutiliza el patrón ya probado por X512. Ventajas: un fallo no tumba el kernel, se depura con un depurador normal y se itera sin reiniciar.
* ADR-002 — El contrato con Mesa es la ABI radeon_drm.h de Linux. Se emulan los ioctls DRM de radeon en una biblioteca shim que habla por IPC con el servidor. Así src/gallium/winsys/radeon/drm de Mesa se usa casi sin cambios y no se escribe un winsys nuevo. Esto sustituye a la idea previa de un winsys propio.
* ADR-003 — El verificador de command streams vive dentro del servidor. Es la pieza de seguridad. En userland es más fácil de probar con fuzzing y no puede colgar el kernel.
* ADR-004 — El modesetting se queda en el accelerant/driver radeon_hd. El servidor solo hace render y gestión de memoria. Hay que repartir los registros entre ambos (§6.7).
* ADR-005 — Vulkan 1.0 = puente "Vulkan sobre Gallium" derivado de Lavapipe. No se escribe un driver Vulkan nativo para TeraScale (§8).
* ADR-006 — Ejecución híbrida GPU+CPU. Un solo VkDevice con dos ejecutores. La GPU hace lo que cabe en su hardware; la CPU, con el camino software de Gallium y kernels propios, hace el resto. Esto sustituye a la restricción de v1 "Vulkan solo en Evergreen+": R600/R700 (y R300–R500, con rendimiento bajo) pasan a poder anunciar Vulkan 1.0. La CPU es el oráculo de corrección (§8.1).
* ADR-007 — Ensamblador con referencia en C. Se usa ensamblador extensivamente para x86-32/64 y ARM32/64 en los kernels de §8A, siempre con referencia en C, despacho en tiempo de ejecución y pruebas diferenciales. No se reescribe a mano el JIT de shaders de CPU (lo genera LLVM).
* ADR-008 — El cómputo que falta se compensa por capas, sin prometer lo que el hardware no da. Evergreen+: cómputo nativo. R600/R700: los kernels "tipo mapa" se emulan con la tubería gráfica de la GPU (vertex shader + stream-out, §8.14) y el resto va a la CPU. R300–R500: todo el cómputo en CPU. El ensamblador acelera la CPU por un factor constante en trabajos fijos y no sustituye a la GPU (§8A.7).
* ADR-009 — Un servidor, un enumerador, un backend por familia. radeon_gpud reconoce todo el espectro Radeon (legacy y amdgpu) con una base de datos de ASIC generada a partir de tablas de Mesa y libdrm. Cada tarjeta recibe una instancia del backend de su familia (rg_legacy o rg_gcn), y todas comparten el ciclo de vida de bloques IP (§7A.4).
* ADR-010 — En GCN+ el aislamiento lo da GPUVM, no un verificador. Cada cliente tiene su espacio de direcciones. Se reutiliza el diseño de amdgpu (cursor de tablas, bo_va, gestor de VMIDs) y se prueba con clientes hostiles (§7A.5).
* ADR-011 — Pantalla de GCN+ por el framebuffer del firmware. Hasta nuevo aviso no se implementa DCE/DCN; la GPU renderiza y copia al framebuffer (§7A.8).
1.3 ¿Y las GPUs móviles?
Existen drivers Vulkan abiertos en Mesa para GPUs de móvil: Turnip (Adreno), PanVK (Mali), V3DV (VideoCore) y el driver PowerVR. Ninguno sirve para Radeon, porque es otro silicio. Sí sirven como referencia de cómo se estructura un driver Vulkan 1.0 mínimo sobre el runtime común de Mesa (src/vulkan/runtime). En potencia bruta, un Adreno de 2016 ronda 0,4 TFLOPS, y los actuales superan 2 TFLOPS, lo que es comparable a las Radeon HD 5000/6000 de gama media-alta. Pero lo que decide el soporte Vulkan es el conjunto de funciones de la GPU, no los FLOPS: las móviles se diseñaron con compute, SSBO y enteros desde el principio, y TeraScale solo lo tiene a partir de Evergreen. Entonces hay que encontrar alternativas y soluciones para esto, el uso de assembler es prioritario en esas areas. 


________________


2. Matriz de hardware y objetivos
Familia
	GPUs
	OpenGL en Mesa
	Compute/SSBO en Gallium
	Objetivo
	R300–R500
	Radeon 9500 – X1950
	2.1 (r300)
	No
	GL (Tier 3). Vulkan 1.0 híbrido, casi todo en CPU (Tier 5)
	R600/R700
	HD 2000 – 4000
	3.3 (r600)
	No
	GL (Tier 1). Vulkan 1.0 híbrido: gráficos en GPU, cómputo en CPU (Tier 4)
	Evergreen
	HD 5000
	4.5 (r600)
	Sí
	GL (Tier 2). Vulkan 1.0 híbrido, casi todo en GPU (Tier 4)
	Northern Islands
	HD 6000
	4.5 (r600)
	Sí
	GL y Vulkan 1.0 híbrido (Tier 4)
	R100/R200
	Radeon 7000–9250
	—
	—
	Fuera de alcance (rama Amber de Mesa)
	

Fuente de versiones GL: tabla de características de Mesa ✔ [VERIFICAR contra la versión de Mesa que se use].


Prioridad: Tier 1 (R600) → Tier 2 (Evergreen) → Tier 3 (r300) → Tier 4 (Vulkan híbrido en R600/R700/Evergreen/NI) → Tier 5 (Vulkan híbrido en R300–R500). Evergreen va pronto porque comparte driver con R600 y es donde el ejecutor GPU de Vulkan cubre más.


Solo PCIe en el primer ciclo. AGP queda diferido (Haiku tiene módulo de bus AGP [VERIFICAR]).


Plataformas de CPU: x86-64 (prioridad 1), x86-32 (2), ARM64 (3, experimental) y ARM32 (4, solo camino CPU). Detalles en §6.10.


Familias amdgpu (GCN, Vega, RDNA; CDNA queda fuera): el mismo servidor las atiende con otro backend. Tabla de cobertura y prioridades en §7A.2; fases en §9.3.


________________


3. Arquitectura
 App OpenGL ─► libGL (Mesa hgl/EGL) ─► Gallium r300g / r600g


 App Vulkan ─► libvulkan (loader) ─► lvp-hw ─► planificador ─┬─► ejecutor GPU: Gallium r600g / r300g


                                                            └─► ejecutor CPU: llvmpipe / draw + rg_asm


                                              │


                            winsys radeon (Mesa, sin cambios salvo mmap)


                                              │  drmCommandWriteRead(...)


                              ┌───────────────▼───────────────┐


                              │ libdrm_haiku (shim)           │  emula ioctls radeon_drm.h


                              └───────────────┬───────────────┘


                                              │ BPort + áreas compartidas


                              ┌───────────────▼───────────────┐


                              │ radeon_gpud (servidor, userland)│  BOs, GART/VRAM, CS verifier,


                              │                               │  anillo CP, fences, IRQ


                              └───────────────┬───────────────┘


                                              │ ioctl helper


                              ┌───────────────▼───────────────┐


                              │ radeon_hd (kernel, extendido) │  MMIO, IRQ→sem, traducción


                              └───────────────┬───────────────┘  física, config PCI


                                              ▼


                                     GPU (MMIO / VRAM / GART)


 Pantalla: accelerant radeon_hd (modos, cursor, scanout) ◄─► app_server


Este diagrama es el de las familias legacy. El servidor es único y atiende además a las familias amdgpu con otro backend, otro conjunto de ioctls emulados y otro ICD de Vulkan (§7A).


________________


4. Componentes
ID
	Componente
	Lenguaje
	Ubicación sugerida
	C1
	radeon_hd extendido (helper ioctls)
	C/C++
	árbol de Haiku
	C2
	radeon_gpud
	C++
	repo propio, luego src/servers/ o paquete
	C3
	libdrm_haiku (shim)
	C
	repo propio
	C4
	Mesa: parches Haiku + receta HaikuPorts
	meson
	HaikuPorts / fork de Mesa
	C5
	lvp-hw (puente Vulkan híbrido: analizador, planificador, rastreador de recursos)
	C
	fork de Mesa
	C6
	Paquete de firmware
	—
	HaikuPorts
	C7
	rg_asm (kernels en ensamblador/intrínsecos con referencia en C)
	C / GNU as
	repo propio
	C8
	rg_asic (enumerador y base de datos de ASIC, generada en compilación)
	C++ / script
	repo propio
	C9
	rg_gcn (backend de familias amdgpu: bloques IP, GPUVM, anillos) y capa de ioctls amdgpu_drm.h
	C++
	repo propio, o integración del trabajo de X512 (§7A.11)
	

________________


5. Contrato del driver helper (C1)
El kernel solo hace lo que no se puede hacer en userland. Nombres de API de kernel Haiku a usar [VERIFICAR firmas en KernelExport.h]: map_physical_memory, install_io_interrupt_handler, release_sem_etc (con B_DO_NOT_RESCHEDULE), get_memory_map, lock_memory, módulo pci_module_info.


Ioctl
	Entrada
	Salida
	Notas
	GET_INFO
	—
	ids PCI, revisión, mmio_phys/size, vram_phys/size
	

	MAP_MMIO
	—
	area_id
	Reutilizar lo que ya hace radeon_hd con su área compartida
	MAP_VRAM
	—
	area_id (write-combining)
	Idem
	TRANSLATE_PHYS
	area_id, offset, len
	lista (phys, len)
	Necesaria para llenar la tabla GART
	IRQ_ATTACH
	sem_id, status_reg, mask
	—
	El handler comprueba si la interrupción es nuestra, la enmascara y libera el semáforo; el servidor la rearma
	PCI_CFG
	offset, len, rw
	datos
	Con límites; sin acceso a BARs ajenos
	

Comprueba primero qué hace ya radeon_hd con interrupciones y con el arranque de AtomBIOS (§6.7) y no dupliques.


________________


6. Las partes difíciles, resueltas
6.1 Protocolo IPC (shim ⇄ servidor)
Mensajes de tamaño fijo por port_id; los datos grandes viajan en áreas compartidas.
enum rg_msg {
    RG_HELLO,
    RG_INFO,
    RG_BO_CREATE,
    RG_BO_MMAP,
    RG_BO_WAIT_IDLE,
    RG_BO_SET_TILING,
    RG_BO_GET_TILING,
    RG_BO_BUSY,
    RG_BO_CLOSE,
    RG_CS,
    RG_FENCE_WAIT
};


struct rg_bo_create {
    uint64 size;
    uint32 alignment;
    uint32 domains;
    uint32 flags;
};


struct rg_bo_reply {
    uint32 handle;
    area_id area;
    uint64 gpu_addr;
};


struct rg_cs {
    uint32 ib_dwords;
    uint32 n_relocs;
    area_id ib_area;
    area_id reloc_area;
    uint32 ring;
    uint32 flags;
};


struct rg_cs_reply {
    int32 status;
    uint64 fence;
};
Cada cliente = un team_id con su tabla de handles. El servidor comprueba que el handle pertenece al cliente en cada mensaje. Al morir un cliente (B_TEAM_DELETED), se liberan sus BOs.
6.2 Shim de ioctls DRM (C3)
Mesa llama a drmCommandWriteRead(fd, DRM_RADEON_*, ...), drmIoctl y mmap(fd,…). El shim:


* Entrega un fd falso por cliente y convierte cada comando a un mensaje IPC.
* Ioctls que usa el winsys radeon de Mesa y que hay que cubrir ✔: DRM_RADEON_INFO, GEM_CREATE, GEM_MMAP, GEM_WAIT_IDLE, GEM_BUSY, GEM_SET_TILING, GEM_GET_TILING, GEM_SET_DOMAIN, CS, y DRM_IOCTL_GEM_CLOSE. [VERIFICAR el conjunto exacto en radeon_drm_winsys.c, radeon_drm_bo.c, radeon_drm_cs.c]
* RADEON_INFO_* que Mesa consulta al arrancar (id de dispositivo, nº de pipes, TILING_CONFIG, ACCEL_WORKING, etc.): responder desde GET_INFO y de los registros de configuración de la GPU.
* mmap: el winsys llama a mmap directamente sobre el fd. Haiku no puede mapear así un fd falso. Parche mínimo en Mesa para Haiku: redirigir a una función del shim que haga clone_area del area_id del BO. (X512 hizo algo equivalente en libdrm con drmMmap → create_area ✔.)
* Cabeceras: usa radeon_drm.h de libdrm/UAPI como contrato (cabecera MIT) ✔ [VERIFICAR licencia en el archivo concreto].
6.3 Gestión de memoria (BOs, VRAM, GART)
* VRAM: asignador de rangos (lista de huecos o buddy), alineación por BO. El aperture visible por CPU puede ser menor que la VRAM total; los BOs que la CPU mapea se piden en la parte visible.
* GART: la GPU ve la RAM del sistema mediante una tabla de páginas que el servidor escribe. Para cada BO de dominio GTT: crear un área B_FULL_LOCK, pedir las direcciones físicas con TRANSLATE_PHYS, escribir una entrada por página, vaciar la caché/TLB de la GART y devolver la dirección GPU dentro de la ventana GTT.
* Política v1 (sin evicción): si piden VRAM y no cabe, devolver GTT. Evicción real se deja para Fase 8.
* Dominios: RADEON_GEM_DOMAIN_VRAM, GTT, CPU ✔.
* Tiling: guardar los flags de tiling por BO y exponerlos a Mesa; el verificador los usa para validar pitch/bank en los registros de color/profundidad.
6.4 Verificador de command streams (la pieza crítica)
Problema. En R300–R700 los paquetes PM4 llevan direcciones GPU absolutas, y no hay VM por proceso. Un cliente podría escribir en la memoria de otro proceso, o en el kernel, simplemente poniendo una dirección arbitraria.


Solución. Mesa ya emite relocaciones: en vez de la dirección, el stream lleva un paquete NOP con el índice de un BO en una lista aparte (chunk RELOCS). El servidor:


1. Recibe el IB (chunk IB) y la lista de relocs. Cada reloc referencia un handle del mismo cliente (si no es suyo, rechazo).
2. Recorre el IB paquete a paquete (PM4 tipo 0, 2 y 3).
3. Para cada escritura de registro (SET_CONFIG_REG, SET_CONTEXT_REG, SET_RESOURCE, SET_ALU_CONST…, o tipo 0 en r300) consulta una tabla de registros permitidos:
   * Libre: el registro no puede producir accesos a memoria (estado del rasterizador, etc.).
   * Con reloc: contiene una dirección o base; el paquete debe estar precedido por su NOP de reloc. Se sustituye el valor por gpu_addr(BO) + offset, comprobando offset + tamaño ≤ size(BO).
   * Prohibido: registros del bus, memory controller, interrupciones, etc. → el CS se rechaza.
4. Para DRAW, DMA, COPY y EVENT_WRITE_*/SURFACE_SYNC, verifica que las direcciones y tamaños caen dentro de los BOs referenciados.
5. Valida tiling y pitch de buffers de color/profundidad para que el rasterizador no escriba fuera del BO (cálculo de tamaño mínimo por formato y alineación).
6. Si todo pasa, copia el IB ya parcheado al anillo del CP (o a un IB propio del servidor). Nunca ejecutes el buffer del cliente: lo copias antes de validarlo, para evitar TOCTOU.


Construcción de las tablas. Derívalas de r600d.h/evergreend.h (MIT, de Mesa) y de los PDF de AMD. Haz una lista "por defecto prohibido" y ve habilitando los registros que Mesa realmente emite, que se obtienen grabando los streams de un Mesa en Linux de referencia (modo de depuración de Mesa / R600_DEBUG) [VERIFICAR opciones]. En Linux la misma idea se implementa con bitmaps "reg_safe_bm" en r600_cs.c, evergreen_cs.c y r100/r300 ✔ — úsalo para entender la estructura, no para copiar.


Fuzzing. Un generador de IBs aleatorios + mutaciones de IBs reales debe correr contra el verificador en userland, sin GPU, con sanitizers. Criterio: ningún stream rechazado puede alcanzar la GPU; ninguno aceptado puede direccionar fuera de sus BOs (propiedad comprobada con un modelo de memoria simulado).
6.5 Anillo de comandos, fences e interrupciones
* R600/Evergreen: anillo del CP + anillo de interrupciones (IH). La fence se escribe con un evento de fin de pipeline (EVENT_WRITE_EOP) a una dirección del servidor y se compara en CPU; la IRQ solo despierta al servidor.
* R300: fence mediante SCRATCH_REG escrito desde el anillo ✔ [VERIFICAR].
* RG_FENCE_WAIT espera con un semáforo con timeout, y si vence hace polling. Si una fence no avanza (GPU colgada): resetear la GPU (soft reset), marcar los contextos afectados como perdidos y responder -ECANCELED al cliente.
6.6 Firmware
* R300–R500 usan microcódigo de CP (R300_cp.bin, R420_cp.bin, R520_cp.bin, RS690_cp.bin…), R600/R700/Evergreen usan *_pfp.bin, *_me.bin, *_rlc.bin, y los Northern Islands además *_mc.bin ✔ [VERIFICAR nombres exactos en linux-firmware/radeon].
* Distribuye en un paquete .hpkg aparte (C6) bajo la licencia redistribuible de linux-firmware (revisa los términos en el repo). El servidor los carga desde /boot/system/data/firmware/radeon/ [VERIFICAR ruta].
6.7 Reparto de registros entre radeon_hd y el servidor
Riesgo: dos entidades escribiendo el mismo hardware. Regla:


* radeon_hd/accelerant: CRTC, PLL de pantalla, AtomBIOS de modesetting, cursor, conectores.
* Servidor: CP, anillos, GART, memory controller (solo lectura de MC_FB_LOCATION, no reconfigurar si ya lo dejó el BIOS/radeon_hd), IH, sync/reset del bloque 3D.
* Primera tarea de la Fase 1: leer radeon_hd y listar qué toca ya. Si ejecuta ASIC_Init de AtomBIOS, el servidor debe asumir ese estado. Documenta el resultado en docs/REGISTER_OWNERSHIP.md.
6.8 Secuencias de inicialización (orden de referencia)
Sirven de guía de orden; los nombres son funciones del driver de Linux ✔: r600_startup(), evergreen_startup(), r300_startup(). Orden típico: GART → programar el memory controller → configuración GPU (pipes, backends, tiling) → IRQ → cargar microcódigo → iniciar CP y anillo → prueba de anillo (escribir en un scratch y leerlo) → prueba de IB.
6.9 Presentación en pantalla
* v1 (simple): renderizar a un BO, leerlo a un BBitmap y mostrarlo (copia por CPU). Lento pero correcto, y suficiente para validar.
* v2: copia GPU→GPU hacia un BO de scanout compartido con app_server, con page flip o blit.
* Para Vulkan: capa WSI sobre vulkan-wsi-layer ✔, con extensión de superficie propia de Haiku (X512 ya lo planteó).
6.10 Plataformas: x86-32/64 y ARM32/64
Aspecto
	x86-64 / x86-32
	ARM64 / ARM32
	Orden de memoria
	Fuerte (TSO); sfence tras escrituras WC
	Débil: barreras dmb/dsb explícitas antes de tocar registros (K5, K6)
	Coherencia de DMA por PCIe
	Coherente en general: páginas GART con snoop
	A menudo no coherente: hay que mantener la caché a mano (DC CVAC/DC CIVAC) o mapear el GTT sin caché (K5)
	Mapeo del BAR de VRAM
	Write-combining
	Debe ser Normal-NC; si es memoria de dispositivo, los accesos SIMD o no alineados fallan [HUMANO]
	Espacio de direcciones
	x86-32: ~2–3 GB utilizables; limitar mapeos de BOs y evitar mapear todo el BAR
	64 bits sin problema; ARM32 igual que x86-32
	Interrupciones
	MSI/MSI-X o INTx
	MSI depende del controlador PCIe de la placa [VERIFICAR]
	Estado en Haiku
	Soportado
	ARM64/ARM32 en desarrollo [VERIFICAR estado actual]
	

Consecuencias de diseño:


1. El helper (§5) expone una bandera host_coherent por plataforma. El planificador (§8.6) la usa para elegir entre GTT coherente y GTT no coherente.
2. En plataformas no coherentes, el ejecutor CPU trabaja siempre sobre una copia en memoria cacheada (staging) y no directamente sobre el BO.
3. Todo acceso a MMIO pasa por los accesores de K6; no se escribe MMIO con punteros normales.
4. Prioridad de plataforma: x86-64 primero, luego x86-32, y ARM64 como objetivo experimental. ARM32 se limita al camino CPU y a los kernels NEON; una Radeon PCIe en ARM32 es poco probable.


________________


7. Ruta OpenGL (Mesa)
1. Compilar Mesa para Haiku con -Dgallium-drivers=r300,r600,softpipe,llvmpipe y la plataforma Haiku ✔ [VERIFICAR opciones actuales de meson_options.txt].
2. Conectar el frontend hgl ✔ (src/gallium/frontends/hgl) a un pipe_screen real vía winsys radeon. Si hgl solo conoce un winsys software, añadir un modo "hardware" que cree el screen con radeon_drm_winsys_create().
3. Mantener el fallback a software cuando el servidor no arranque.
4. Receta HaikuPorts para el paquete con los drivers activados.


________________


7A. Familias amdgpu (GCN, Vega, RDNA, CDNA): un solo servidor para todo el espectro Radeon
7A.1 Objetivo y principio
El servidor radeon_gpud debe reconocer cualquier Radeon que cubra el driver amdgpu de Linux, además de las legacy de §2, y atender cada tarjeta con el backend de su familia. Lo que se reutiliza de amdgpu es su arquitectura, no su código:


* Bloques IP como unidades composables con un ciclo de vida común (§7A.4). Es la estructura anidada del driver: un dispositivo contiene bloques, y estos pueden contener subbloques o motores.
* GPUVM: tablas de páginas jerárquicas por cliente, que se recorren con un cursor a través de los niveles (§7A.5). Es la parte recursiva por excelencia del driver.
* Anillos, fences por writeback, planificador con dependencias, cola de interrupciones (IH) con identificadores de cliente, y descubrimiento de IP en las familias nuevas.


El servidor sigue siendo uno solo. Las familias legacy (§6) pasan a expresarse con el mismo ciclo de vida de bloques IP, de modo que reinicio, suspensión y reset se tratan igual en todas.
7A.2 Cobertura por familia
Los nombres comerciales son orientativos [VERIFICAR contra amd_family.h de Mesa y amdgpu.ids de libdrm].


Familia
	Arquitectura (nivel GFX)
	Ejemplos
	Backend
	OpenGL
	Vulkan
	Prioridad
	R300–R500
	Pre-TeraScale
	9500–X1950
	rg_legacy
	r300g
	lvp-hw híbrido
	Tier 5
	R600/R700
	TeraScale 1
	HD 2000–4000
	rg_legacy
	r600g
	lvp-hw híbrido
	Tier 4
	Evergreen / Northern Islands
	TeraScale 2/3
	HD 5000/6000
	rg_legacy
	r600g
	lvp-hw híbrido
	Tier 4
	Southern Islands
	GCN 1 (GFX6)
	HD 7700–7900
	rg_gcn
	radeonsi
	RADV (Vulkan 1.0–1.2)
	Opcional (G8)
	Sea Islands
	GCN 2 (GFX7)
	R9 290, APUs Kaveri/Kabini
	rg_gcn
	radeonsi
	RADV (Vulkan 1.0–1.2)
	Opcional (G8)
	Volcanic Islands / Polaris
	GCN 3–4 (GFX8)
	R9 285/380, Fury, RX 400/500
	rg_gcn
	radeonsi
	RADV (Vulkan 1.4 Nativo)
	G2 (primer objetivo)
	Vega y APUs Ryzen
	GCN 5 (GFX9)
	Vega 56/64, Radeon VII, Raven/Picasso/Renoir
	rg_gcn
	radeonsi
	RADV (Vulkan 1.4 Nativo)
	G6
	RDNA 1 / 2
	GFX10.1 / 10.3
	RX 5000 / 6000
	rg_gcn
	radeonsi
	RADV (Vulkan 1.4 Nativo)
	G7
	RDNA 3 / 3.5
	GFX11 / 11.5
	RX 7000, APUs recientes
	rg_gcn
	radeonsi
	RADV (Vulkan 1.4 Nativo)
	G7
	RDNA 4
	GFX12
	RX 9000
	rg_gcn
	radeonsi
	RADV (Vulkan 1.4 Nativo)
	G7, según madurez de Mesa y firmware
	CDNA
	GFX9.x
	MI100/200/300
	—
	—
	—
	Fuera de alcance (sin gráficos; ROCm/KFD)
	Requisitos Obligatorios de Vulkan 1.4 en RADV (GCN4+, RDNA1–4):
* Expansión de características obligatorias: vulkanMemoryModel, vulkanMemoryModelDeviceScope, dynamicRendering, extendedDynamicState1/2/3, pushDescriptors, scalarBlockLayout, maintenance4/5/6.
* Sincronización por Timelines: VK_KHR_timeline_semaphore e infraestructura de objetos syncobj en kernel/servidor.
* Capacidades de pipeline: Soporte completo de SSBO, cómputo masivo asíncrono, imágenes de almacenamiento de 32 y 64 bits, y operaciones atómicas flotantes.
7A.3 Detección: un enumerador y una base de datos de ASIC (rg_asic)
def enumerar():


    for disp in pci_devices(vendor=0x1002, class_in=[VGA, Display, 3D]):


        dev_id = (disp.device, disp.revision, disp.subsystem)


        if dev_id in tabla_legacy:


            asic = asic_legacy(dev_id)    # ids de r300/r600 de Mesa y radeon_pciids de Linux


        elif dev_id in tabla_amdgpu:


            asic = asic_gcn(dev_id)       # pciidlist de amdgpu, amdgpu.ids de libdrm, radeonsi_pci_ids.h de Mesa


        else:


            asic = DESCONOCIDA            # sin aceleración: solo framebuffer; registrar el ID en el log


        


        # Familias nuevas: confirmar con el descubrimiento de IP (tabla en VRAM o firmware)


        # y rellenar versiones de bloques IP


        registrar(asic)


* La base de datos no se escribe a mano: un script la genera en la compilación a partir de include/pci_ids/*.h de Mesa, data/amdgpu.ids de libdrm y src/amd/common/amd_family.h ✔. Un test de CI comprueba que todos los IDs de esas fuentes quedan clasificados y publica el informe de cobertura.
* Una GPU desconocida nunca recibe aceleración. Se deja el framebuffer y se avisa en el log.
* Descriptor mínimo: {familia, nivel_gfx, ids, es_apu, tipo_vram, características{vm, sdma, mec, psp, smu}, ip[]{tipo, mayor, menor, rev}}.
* El enumerador crea una instancia de backend por tarjeta. Los sistemas con varias funcionan sin transferencias entre GPUs en v1.
7A.4 Bloques IP: ciclo de vida común y descubrimiento en RDNA 3/4
struct rg_ip_block {


    enum ip_type type;


    const char *name;


    int (*early_init)(struct rg_device *adev);


    int (*sw_init)(struct rg_device *adev);


    int (*hw_init)(struct rg_device *adev);


    int (*late_init)(struct rg_device *adev);


    int (*suspend)(struct rg_device *adev);


    int (*resume)(struct rg_device *adev);


    int (*hw_fini)(struct rg_device *adev);


    int (*sw_fini)(struct rg_device *adev);


    bool (*is_idle)(struct rg_device *adev);


    int (*soft_reset)(struct rg_device *adev);


};


Orden de arranque: COMMON/NBIO → GMC → IH → PSP → SMU → GFX → SDMA. No se implementan en v1: DCE/DCN (pantalla), VCN/UVD/VCE (vídeo), KFD, SR-IOV, RAS y gestión de energía avanzada.


* Descubrimiento de IP por Tabla Firmware (amdgpu_discovery) en RDNA 2/3/4 (GFX10.3/GFX11/GFX12):
   * En RDNA 2+, los registros de bloque no tienen direcciones fijas predefinidas. Durante early_init, radeon_gpud lee e interpreta la tabla de descubrimiento ubicada en VRAM o expuesta por el firmware para determinar dinámicamente direcciones y versiones de GFX, SDMA, PSP, SMU y GMC.
   * Inicialización de bloques PSP (Platform Security Processor) v12/v13/v14 para el handshake de firmware en GPUs RDNA modernas.
   * Gestión de anillos SDMA v5/v6 y colas de cómputo MEC (Micro Engine Compute) para despacho concurrente y asíncrono exigido en Vulkan 1.4.
7A.5 Memoria: GPUVM por cliente (sustituye al verificador en GFX6+)
Dos modelos detrás de la misma API de BOs:


Característica
	Legacy (§6.3–6.4)
	GCN y posteriores
	Aislamiento
	Verificador de CS + relocaciones
	GPUVM: un espacio de direcciones por cliente
	Direcciones en los comandos
	Absolutas, parcheadas por el servidor
	Virtuales del cliente, sin parchear
	Protección
	El software valida cada paquete
	El hardware lo valida mediante tablas de páginas
	

Diseño del GPUVM:


* Tablas jerárquicas (4 niveles en GFX9+ / RDNA). Dirección virtual de 48 bits manejada mediante AMDGPU_VA_OP con banderas de lectura/escritura/ejecución.
* Máquina de estados por mapeo (bo_va): invalid → evicted → moved → idle.
* VMIDs limitados y gestor de VMIDs: reasignación por LRU con flush de TLB al cambiar de cliente.
* Fallos de página: llegan por IH. Se identifica el cliente y se destruye su contexto aisladamente.
7A.6 Capa de compatibilidad con Mesa para amdgpu y Requisitos Vulkan 1.4
* drmGetVersion() devuelve name = "amdgpu".
* Se emula amdgpu_drm.h manteniendo libdrm_amdgpu sin cambios sobre drmCommandWriteRead.
* Ioctls extendidos para Vulkan 1.4:
   * Syncobj Timelines: Soporte de DRM_IOCTL_SYNCOBJ_TIMELINE_WAIT, DRM_IOCTL_SYNCOBJ_TIMELINE_SIGNAL y DRM_IOCTL_SYNCOBJ_TRANSFER para mapear VK_KHR_timeline_semaphore.
   * Memory Allocator: Direccionamiento de 48 bits mediante AMDGPU_VA_OP, y banderas AMDGPU_GEM_CREATE_CPU_GTT_USABLE.
   * Dev Info Ampliado (AMDGPU_INFO_DEV_INFO): Exponer recuento real de CUs, arquitectura GFX8–GFX12, límites de registros de despacho de cómputo y configuración de memoria invisible.
7A.7 Drivers de Mesa por familia y arreglo de ICDs
* Familias GCN+: radeonsi (OpenGL) y RADV (Vulkan 1.4 nativo).
* Familias legacy: r300g/r600g y lvp-hw.
* Dos ICDs de Vulkan (libvulkan_radeon y libvulkan_lvphw).
7A.8 Pantalla en GCN+ (decisión)
ADR-011: Framebuffer que deja el firmware (UEFI GOP o VESA). La GPU renderiza en un BO y copia al framebuffer.
7A.9 Firmware y arranque por generación
Generación
	Carga del firmware
	Notas
	GFX6–7
	Directa (CP, RLC, MC)
	MC necesario
	GFX8 (Polaris)
	Directa, o vía SMU
	Primer objetivo
	GFX9 (Vega, APUs)
	PSP
	Handshake con el procesador de seguridad
	GFX10+ (RDNA 1–4)
	PSP v12/v13/v14 + SMU
	Descubrimiento de IP por tabla VRAM/firmware
	7A.10 Qué se reutiliza de amdgpu y qué no
* Se reutiliza la especificación lógica: formatos de paquetes PM4/SDMA, mapas de registros de amdgpu_asic.h, algoritmos de GPUVM y secuencias de inicialización de IP.
* Se reimplementa en C++ para Haiku: las estructuras del servidor, el gestor IPC, las abstracciones de hilos y semáforos, y las llamadas al helper de kernel.
7A.11 Trabajo existente: no duplicar
Evaluación del trabajo previo de X512 (RadeonGfx):


* Analizar la compatibilidad de su shim de libdrm y su servidor con el diseño de rg_gcn.
* Registrar en docs/DECISIONS.md si se adopta su código base de GCN como punto de partida para la pista G o si se refactoriza sobre la arquitectura unificada de radeon_gpud.


________________


8. Vulkan 1.0 híbrido: la GPU hace lo que puede y la CPU el resto (lvp-hw)
8.1 Idea básica
No escribas un driver Vulkan desde cero para GPUs antiguas. Usa Lavapipe (lvp), el driver Vulkan por software de Mesa. Lavapipe traduce Vulkan a llamadas a Gallium (pipe_screen, pipe_context). Sustituye la pantalla software por una pantalla híbrida que envía las operaciones de renderizado a Gallium hardware (r600g o r300g) y las operaciones no soportadas (cómputo, SSBO, pipeline derivado) a un ejecutor por CPU alimentado por llvmpipe o por kernels en ensamblador (rg_asm).
8.2 ¿Qué ejecuta la GPU y qué ejecuta la CPU?
Operación Vulkan
	R300–R500
	R600/R700
	Evergreen / Northern Islands
	Pass de render con color y profundidad
	GPU
	GPU
	GPU
	Samplers, texturas 2D, filtrado
	GPU
	GPU
	GPU
	Shaders de vértices y fragmentos
	GPU (adaptado por NIR)
	GPU
	GPU
	Shaders de geometría
	CPU
	GPU
	GPU
	Shaders de computación (vkCmdDispatch)
	CPU
	Emulado por GPU (§8.14) o CPU
	GPU
	Buffers de almacenamiento (SSBO)
	CPU
	CPU
	GPU
	Transferencias de memoria (vkCmdCopyBuffer)
	GPU (CP DMA)
	GPU (CP DMA)
	GPU (CP DMA)
	Consultas de oclusión
	GPU
	GPU
	GPU
	8.3 Descomposición de pasadas de comandos
Cuando el cliente registra un VkCommandBuffer:


1. El rastreador analiza las barreras y las operaciones del buffer.
2. Si un comando no está soportado por el backend de la GPU activa, se inserta una transición de memoria (flush de caché VRAM → GTT) y se conmuta el contexto activo al ejecutor CPU.
3. El planificador consolida los comandos continuos para minimizar los cambios de contexto entre GPU y CPU.
8.4 Sincronización entre ejecutores
* Semáforos y Fences internos: La CPU espera a la GPU comprobando las fences devueltas por RG_CS.
* La GPU espera a la CPU mediante el envío de un comando de espera en el anillo sobre un valor de memoria GTT escrito por la CPU.
* Las transiciones implican invalidación de datos: la CPU escribe en buffer staging cacheado; un comando DMA de la GPU copia de staging a VRAM antes de ejecutar la pasada de dibujado.
8.5 Gestión de recursos compartidos (CPU/GPU)
* Todo VkDeviceMemory se asigna a través de la API del servidor (RG_BO_CREATE).
// Pseudocódigo del planificador de recursos compartidos
typedef struct {
    uint64_t gpu_address;
    void* host_ptr;
    bool is_coherent;
    bool is_vram_local;
} rg_resource_t;


void rg_planner_sync_resource(rg_resource_t* res, bool writing_from_cpu) {
    if (!res->is_coherent) {
        if (writing_from_cpu) {
            // Flush CPU cache to system RAM / GTT
            rg_arch_flush_cpu_cache(res->host_ptr, res->size);
        } else {
            // Invalidate CPU cache before reading back
            rg_arch_invalidate_cpu_cache(res->host_ptr, res->size);
        }
    }
}
* Los buffers que necesiten acceso CPU/GPU simultáneo se ubican en dominio GTT con páginas fijadas (B_FULL_LOCK).
* Las imágenes de textura con tiling HW se des-entrelazan en la CPU usando funciones aceleradas por SIMD si la CPU necesita leer de ellas directamente.
8.6 El Planificador (rg_planner)
Un componente ligero dentro del ICD Vulkan que:


1. Inspecciona el grapo de dependencias del Command Buffer.
2. Determina el costo de migración de datos (VRAM ↔ System RAM).
3. Asigna la ejecución de cada VkCmd al motor GPU o CPU intentando maximizar el tiempo que la GPU permanece en memoria local.
8.7 El oráculo CPU
Cualquier función ejecutada en la GPU puede verificarse contra el ejecutor software puro ejecutando con la variable de entorno RG_VK_MODE=cpu-only. Esto permite validar la precisión de la compilación de NIR y las transiciones de formato sin dudar del hardware.
8.8 Compilador de shaders para lvp-hw
* Entrante: SPIR-V.
* spirv_to_nir convierte el SPIR-V a representación intermedia NIR.
* Si el comando va a la GPU: las optimizaciones de NIR aplican lowering según las capacidades expuestas por r600g/r300g (por ejemplo, descomposición de enteros o emulación de float64).
* Si el comando va a la CPU: se entrega a llvmpipe para generación de código de máquina nativo.
8.9 Banderas de características Vulkan expuestas
El driver anuncia únicamente las características de Vulkan 1.0 Mínimo:


* robustBufferAccess: falso.
* fullDrawIndexUint32: verdadero.
* imageCubeArray: según soporte de familia.
* independentBlend: verdadero.
* geometryShader: verdadero (vía GPU en R600+, CPU en R300).
* tessellationShader: falso en v1.
8.10 Formatos de imagen soportados
* Formatos de color estándar: R8G8B8A8_UNORM, R8G8B8A8_SRGB, B8G8R8A8_UNORM, R5G6B5_UNORM_PACK16.
* Formatos de profundidad/stencil: D16_UNORM, D24_UNORM_S8_UINT, D32_SFLOAT.
* Formatos comprimidos BC1-BC5 mapeados directamente a hardware.
8.11 Soporte de WSI (Window System Integration)
* Implementación de VK_KHR_surface y VK_HAIKU_bmessenger_surface.
* Presentación mediante la copia del BO de render al área de intercambio expuesta por la ventana de Haiku (BScreen / BView).
8.12 Manejo de errores de ejecución
* Si la GPU genera una falla durante la ejecución de un Command Buffer (p. ej. timeout de fence), el planificador invalida el contexto GPU y redirige las subsecuentes pasadas a CPU notificando VK_ERROR_DEVICE_LOST si no es posible la recuperación transparente.
8.13 Límites de rendimiento esperados
* R300–R500: ~5–15 FPS en demos sencillas de Vulkan 1.0 debido a la sobrecarga del camino CPU.
* R600/R700: 30+ FPS en cargas gráficas puras.
* Evergreen/Northern Islands: Rendimiento cercano a nativo OpenGL para cargas de trabajo balanceadas.
8.14 Emulación de Compute en R600/R700 mediante Graphics Pipeline
* Para kernels de tipo "mapa" (operaciones elemento a elemento sobre buffers):
   1. Se configura un shader de vértices que lee los datos de entrada desde un Buffer Resource.
   2. Se ejecuta un DrawArrays de tipo POINT con el número de elementos igual a las dimensiones del Dispatch.
   3. La salida se dirige mediante Stream-Out (Transform Feedback) hacia el buffer de destino.
8.14.3 Clasificador de Compute Shader (GPU vs CPU)
enum rg_dispatch_target {
    DISPATCH_TARGET_GPU_COMPUTE,   // Compute nativo (Evergreen+)
    DISPATCH_TARGET_GPU_GRAPHICS,  // Emulación por Pipeline Gráfico (R600/R700)
    DISPATCH_TARGET_CPU_LLVMPIPE   // CPU Fallback (R300-R500 o características avanzadas)
};


enum rg_dispatch_target rg_classify_compute(const struct nir_shader *nir, enum rg_family family) {
    if (family < R600) return DISPATCH_TARGET_CPU_LLVMPIPE;
    if (family >= EVERGREEN) return DISPATCH_TARGET_GPU_COMPUTE;
    
    // R600/R700: Evaluar si el kernel utiliza memoria compartida (LDS/Group Memory)
    if (nir->info.shared_size > 0) {
        return DISPATCH_TARGET_CPU_LLVMPIPE; // Fallback a CPU por falta de LDS nativo en compute simple
    }
    return DISPATCH_TARGET_GPU_GRAPHICS;
}
* Kernels complejos con memoria compartida local (Group Memory) se derivan directamente al ejecutor CPU.


________________


8A. Estrategia de ensamblador (§8A completo)
8A.1 Objetivo
Acelerar las rutinas clave de conversión de formatos de píxeles, des-entrelazado (detiling) de texturas, desempaque de vértices y copia de memoria en la CPU para minimizar la sobrecarga en la ejecución híbrida GPU+CPU de Vulkan y Gallium.
8A.2 Alcance del ensamblador
El ensamblador no sustituye la generación de código para shaders de CPU (realizada por LLVM/llvmpipe). Se restringe a kernels fijos y bien definidos:
1. Conversión de formatos de color y swizzling (p. ej. ARGB8888, NV12, RGBA32F).
2. Tiling y Detiling de texturas con layouts de TeraScale y GCN a lineales.
3. Empaque y desempaque de buffers de atributos de vértices.
4. Barridos de memoria y operaciones de copia optimizadas para sistemas sin coherencia de caché DMA.
8A.3 Requisitos de implementación
Todo kernel en ensamblador debe cumplir estrictamente tres condiciones:
1. Referencia en C: Una versión equivalente escrita en C puro, totalmente portable y clara.
2. Despacho Dinámico: Selección en tiempo de ejecución basada en la detección de características de la CPU (AVX2, SSE4.1, NEON).
3. Prueba de Equivalencia: Test unitario diferencial que valida la exactitud del resultado del kernel contra la referencia en C sobre datos aleatorios.
8A.4 Convención de llamadas y compatibilidad
* Conformidad estricta con System V ABI para x86-64 y AAPCS64 para ARM64.
* Preservación de registros no volátiles según el estándar de la plataforma.
* Prohibido el uso de estado global implícito dentro del código ensamblador.
8A.5 Rutinas en ensamblador x86-64 y ARM64
Ejemplo de kernel de conversión de formato y des-entrelazado optimizado para x86-64 (AVX2):
.global rg_detile_swizzle_avx2
rg_detile_swizzle_avx2:
    # rdi: src, rsi: dst, rdx: count
    vmovdqu (%rdi), %ymm0
    vpshufb %ymm1, %ymm0, %ymm0
    vmovdqu %ymm0, (%rsi)
    ret
Ejemplo equivalente optimizado para ARM64 (NEON):
.global rg_detile_swizzle_neon
rg_detile_swizzle_neon:
    # x0: src, x1: dst, x2: count
    ld1 {v0.16b}, [x0]
    tbl v1.16b, {v0.16b}, v2.16b
    st1 {v1.16b}, [x1]
    ret
8A.6 Accesores MMIO y barreras de memoria en C
#if defined(__x86_64__) || defined(__i386__)
static inline void rg_mmio_write(volatile uint32_t *addr, uint32_t val) {
    *addr = val;
    __asm__ __volatile__("sfence" ::: "memory");
}
static inline uint32_t rg_mmio_read(volatile uint32_t *addr) {
    uint32_t val = *addr;
    __asm__ __volatile__("" ::: "memory");
    return val;
}
#elif defined(__aarch64__)
static inline void rg_mmio_write(volatile uint32_t *addr, uint32_t val) {
    __asm__ __volatile__("dsb st" ::: "memory");
    *addr = val;
}
static inline uint32_t rg_mmio_read(volatile uint32_t *addr) {
    uint32_t val = *addr;
    __asm__ __volatile__("dsb ld" ::: "memory");
    return val;
}
#endif
8A.7 Catálogo de kernels acelerados (K1 a K13)
* K1 — Conversión ARGB8888 a RGBA8888: Reordenamiento de canales para compatibilidad con el framebuffer.
* K2 — Des-entrelazado de Tiling Micro/Macro (TeraScale): Conversión de bloques ordenados por GPU a distribución lineal.
* K3 — Des-entrelazado de Tiling Display/ADDR2 (GCN/RDNA): Transformación de superficies tiled modernas.
* K4 — Empaque de Vértices Float32 a Unorm16/Int16: Compresión de atributos de entrada.
* K5 — Inserción y Barrido de Caches DMA (ARM64 non-coherent): Mantenimiento explícito de líneas de caché de CPU.
* K6 — Copia de Memoria no Temporal (Stream Store): Transferencias de CPU a VRAM mapeada WC evitando contaminación de L3.
* K7 — Conversión YUV420/NV12 a RGBA32F: Decodificación acelerada de planos de vídeo.
* K8 — Relleno de Buffers con Patrones Repetitivos (Memset 128-bit): Limpieza rápida de buffers de comandos y VRAM.
* K9 — Conversión Float16 a Float32: Expansión de texturas y atributos de precisión media.
* K10 — Conversión Float32 a Float16: Compresión de salidas para buffers de acumulación.
* K11 — Extracción de Planos de Profundidad/Stencil (D24S8/D32S8): Separación de datos combinados Z/Stencil.
* K12 — Reducción Atómica en Memoria de Host: Acumuladores paralelos para emulación de contadores atómicos.
* K13 — Swap de Endianness para Registros / IBs: Normalización de palabras de comandos en arquitecturas heterogéneas.
8A.8 Límites del ensamblador
El ensamblador acelera las transferencias y adaptaciones de datos en la CPU por un factor constante (típicamente entre 2x y 4x), pero no reemplaza la capacidad de procesamiento paralelo masivo de una GPU. En cargas limitadas por sombreado complejo de fragmentos en trayectos no acelerados por hardware, la CPU seguirá siendo el cuello de botella principal.
8A.9 Plan de Fases: Pista A (Ensamblador)
* Fase A1 — Implementación en C de Referencia y Suite Diferencial: Codificación de los kernels K1 a K13 en C puro con validación en rg_asm_check.
* Fase A2 — Kernels x86-64 (SSE4.1 / AVX2): Implementación y ajuste de rendimiento en x86-64.
* Fase A3 — Kernels ARM64 (NEON): Implementación y validación diferencial en ARM64.
* Fase A4 — Integración en lvp-hw y Medición: Conexión con el ejecutor híbrido y benchmarking final.
8A. Estrategia de ensamblador
8A.1 Objetivo
Acelerar las rutinas de conversión de formatos, des-entrelazado de tiling y copia de memoria en la CPU para reducir el impacto de las transiciones entre la GPU y la CPU en la ejecución híbrida.
8A.2 Alcance del ensamblador
El ensamblador no reemplaza el compilador de shaders LLVM. Se limita estrictamente a:


5. Swizzling y conversión de formatos de píxeles (p. ej. ARGB8888 a NV12 o FLOAT32).
6. Tiling / Detiling de texturas de formatos propietarios TeraScale a layouts lineales.
7. Operaciones de empaque y desempaque de buffers de vértices.
8. Barridos de memoria optimizados para arquitecturas sin coherencia de caché DMA.
8A.3 Requisitos de implementación
Todo kernel en ensamblador debe cumplir tres reglas absolutas:


4. Referencia en C: Debe existir un equivalente escrito en C puro, portable y legible.
5. Despacho Dinámico: Selección de la función al iniciar mediante la comprobación de banderas CPU (AVX2, SSE4.1, NEON).
6. Prueba de Equivalencia: Un test unitario compara el resultado del kernel en ensamblador contra la versión en C sobre un conjunto de datos aleatorio antes de permitir la ejecución.
8A.4 Convención de llamadas y compatibilidad
* Criterio System V ABI para x86-64 y ARM AAPCS64.
* Preservación de registros según la especificación de la plataforma.
* Sin uso de variables globales dentro del código fuente en ensamblador; todos los punteros y parámetros se transfieren explícitamente en la llamada.
8A.5 Matriz de instrucciones por arquitectura
Arquitectura
	Extensión Mínima
	Extensión Optimizada
	Uso principal
	x86-64
	SSE2
	AVX2 / FMA
	Copia de buffers, Detiling, Conversión de formato
	x86-32
	SSE2
	SSSE3
	Soporte para sistemas legados x86
	ARM64
	NEON
	NEON + DotProd
	Detiling y empaque en placas de desarrollo
	ARM32
	NEON
	NEON
	Camino software en dispositivos integrados
	8A.6 Estructura de archivos de ensamblador
* src/asm/x86/detile_avx2.S
* src/asm/x86/format_convert_sse2.S
* src/asm/arm/detile_neon.S
* src/asm/common/rg_asm_check.c (validador de resultados)
// Accesores MMIO y Barreras para x86/ARM
#if defined(__x86_64__) || defined(__i386__)
static inline void rg_mmio_write(volatile uint32_t *addr, uint32_t val) {
    *addr = val;
    __asm__ __volatile__("sfence" ::: "memory");
}
static inline uint32_t rg_mmio_read(volatile uint32_t *addr) {
    uint32_t val = *addr;
    __asm__ __volatile__("" ::: "memory");
    return val;
}
#elif defined(__aarch64__)
static inline void rg_mmio_write(volatile uint32_t *addr, uint32_t val) {
    __asm__ __volatile__("dsb st" ::: "memory");
    *addr = val;
}
static inline uint32_t rg_mmio_read(volatile uint32_t *addr) {
    uint32_t val = *addr;
    __asm__ __volatile__("dsb ld" ::: "memory");
    return val;
}
#endif
8A.7 Limites del ensamblador
El ensamblador mejora el rendimiento de la CPU en un factor constante (típicamente 2x a 4x para transferencias de datos), pero no puede transformar una CPU en una GPU. Si el embotellamiento del juego o aplicación es el sombreado de píxeles complejo en un camino no soportado por hardware, la aceleración por ensamblador no resolverá la caída de fotogramas.


________________


9. Fases y criterios de aceptación
9.1 Pista principal
Fase 1: Helper de Kernel y Abstracción de Bus
* Trabajo: Extensión de radeon_hd para soportar las llamadas MAP_MMIO, TRANSLATE_PHYS e IRQ_ATTACH. Documentación de propiedad de registros en docs/REGISTER_OWNERSHIP.md.
* CA: El helper expone los punteros físicos correctos y entrega interrupciones al servidor en espacio de usuario.
* Prueba: Test unitario en kernel que mapea y lee el identificador PCI del chip desde userland [HUMANO].
Fase 2: Servidor base y Allocator de Memoria
* Trabajo: Creación de radeon_gpud. Implementación del gestor de BOs (VRAM y GART) sin evicción.
* CA: Peticiones RG_BO_CREATE devuelven handles válidos; la tabla GART se escribe correctamente con direcciones físicas mapeadas.
* Prueba: Programa de prueba que solicita un BO en GTT, escribe datos desde la CPU y verifica mediante el helper las direcciones físicas resultantes.
Fase 3: Verificador de CS (Command Stream) y Seguridad
* Trabajo: Construcción del motor de parsers PM4/Tipo 0 y tablas de registros permitidos para R600 y R300.
* CA: IBs con registros prohibidos o direcciones fuera de rango son rechazados con error. IBs válidos son parcheados con las direcciones absolutas de las BOs.
* Prueba: Suite de fuzzing sobre el verificador en userland ejecutada con AddressSanitizer (ASan); 0 fallos de segmentación o lecturas no válidas permitidas.
Fase 4: Anillo de Comandos y Fences
* Trabajo: Carga de firmware CP/PFP/ME. Formateo e inicialización del anillo de comandos hardware. Mecanismo de espera por fence.
* CA: La GPU ejecuta paquetes NOP y WRITE_DATA emitidos en el anillo. Las fences se incrementan conforme la GPU procesa las instrucciones.
* Prueba: Test de estrés que envía 10.000 IBs simples con escritura en scratch registers y verifica la concordancia de los valores finales [HUMANO].
Fase 5: Shim de DRM y Conexión con Mesa (R600/R700)
* Trabajo: Implementación de libdrm_haiku. Adaptación de las llamadas mmap para usar áreas clonadas. Compilación de Mesa con r600g.
* CA: glxinfo / GLInfo reconoce el renderizador hardware r600.
* Prueba: Ejecución exitosa de glshapes o GLTeapot mostrando geometría 3D acelerada sin corrupción visual [HUMANO].
Fase 6: Soporte para Evergreen y Northern Islands
* Trabajo: Adición de tablas de registros y variantes de microcódigo para GPUs Evergreen/NI.
* CA: r600g inicializa adecuadamente el pipeline 3D en tarjetas Radeon HD 5000/6000.
* Prueba: Demostración funcional de neverball o aplicaciones OpenGL 3.3+ en hardware Evergreen [HUMANO].
Fase 7: Soporte para R300–R500
* Trabajo: Creación del validador de comandos Tipo 0/Packet3 para la arquitectura R300. Integración con r300g de Mesa.
* CA: Renderizado acelerado en tarjetas Radeon 9500 hasta X1950.
* Prueba: Ejecución de pruebas estándar de la suite Mesa Demos (gears, reflect) en una GPU R300 [HUMANO].
Fase 8: Evicción de Memoria y Gestión Avanzada
* Trabajo: Algoritmo de evicción de BOs de VRAM a GTT cuando se agota la memoria local. Gestión de buffers LRU.
* CA: Las aplicaciones no fallan por OUT_OF_MEMORY cuando el consumo total de texturas excede la VRAM física disponible.
* Prueba: Carga iterativa de texturas de gran tamaño que fuercen la rotación entre VRAM y GTT comprobando la estabilidad del servidor.
9.2 Pista A: ensamblador
Fase A1: Rutinas C y Pruebas Diferenciales
* Trabajo: Implementación de todas las funciones de detiling, copia y swizzling en C de referencia.
* CA: Cobertura de tests unitarios al 100% en rg_asm_check.
* Prueba: Ejecución del validador con entradas aleatorias generadas por MERSENNE TWISTER.
Fase A2: Optimización SSE2/AVX2 para x86
* Trabajo: Codificación en ensamblador de los kernels de conversión de formatos y detiling para x86.
* CA: Aceleración mínima de 2.5x con respecto a la versión C en la ejecución de detiling.
* Prueba: Benchmark integrado comparando tiempos de ciclo por píxel.
Fase A3: Optimización NEON para ARM
* Trabajo: Implementación de los kernels de memoria utilizando instrucciones SIMD NEON.
* CA: Correctitud idéntica a la referencia C comprobada por la suite diferencial.
* Prueba: Ejecución de los tests unitarios en entorno de emulación QEMU ARM64 o tabla de desarrollo real.
9.3 Pista G: familias amdgpu (ver §7A)
Paso
	Trabajo
	CA
	G0 Enumerador
	rg_asic: base de datos generada a partir de Mesa y libdrm.
	Clasificación 100% de IDs PCI de Mesa y libdrm.
	G1 Evaluación
	docs/RADEONGFX_EVAL.md: evaluación del trabajo de X512.
	Decisión registrada en docs/DECISIONS.md.
	G2 Polaris (GFX8)
	Bloques IP, firmware, anillo GFX, GPUVM y aislamiento.
	Pruebas de anillo e IB pasan; aislamiento GPUVM verificado [HUMANO].
	G3 Shim amdgpu
	Ioctls de §7A.6, incluidos SYNCOBJ_TIMELINE_*.
	Tests unitarios de syncobj timeline y respuestas de INFO_DEV_INFO.
	G4 radeonsi
	Mesa con radeonsi en Haiku.
	GLInfo muestra radeonsi; GLTeapot dibuja [HUMANO].
	G5 RADV
	RADV (Vulkan 1.4 Nativo) en Polaris/Vega.
	vulkaninfo lista Vulkan 1.4; vkcube anima; subconjunto dEQP-VK.api.vulkan14.* pasa [HUMANO].
	G6 Vega y APUs (GFX9)
	PSP handshake, INFO_DEV_INFO completo.
	G4 y G5 repetidos en Vega [HUMANO].
	G7 RDNA 1–4
	Descubrimiento de IP, PSP v12–v14, colas MEC.
	Vulkan 1.4 nativo en RDNA 2 y RDNA 3/4 [HUMANO].
	G8 SI/CIK (opcional)
	GFX6/7, firmware MC.
	G4/G5 (Vulkan 1.2) en SI/CIK [HUMANO].
	G9 Energía (opcional)
	Mensajería SMU y DPM básico.
	Frecuencias medidas.
	

________________


10. Pruebas y depuración
* Pruebas de Vulkan 1.4 mediante suites dEQP-VK.api.vulkan14.*, dEQP-VK.dynamic_rendering.* y tests de sincronización timeline.
* Monitorización del servidor con la herramienta de depuración integrada: RADEON_LOG_LEVEL=debug radeon_gpud.
* Captura de comandos enviados mediante la activación de dumps de IBs: RG_DUMP_IB=1.
* Verificación de fugas de memoria en las áreas compartidas comprobando el estado de la API de Haiku get_system_info.


________________


11. Estructura de repositorios y entregables


/
├── docs/
│   ├── DECISIONS.md
│   ├── PROGRESS.md
│   └── REGISTER_OWNERSHIP.md
├── src/
│   ├── helper/          # Componente C1 (radeon_hd extendido)
│   │   ├── radeon_hd.cpp
│   │   └── helper_ioctls.h
│   ├── server/          # Componente C2 (radeon_gpud)
│   │   ├── main.cpp
│   │   ├── cs_verifier.cpp
│   │   ├── ring_manager.cpp
│   │   ├── asic/        # Componente C8 (rg_asic)
│   │   ├── legacy/      # Backend TeraScale / R300
│   │   └── gcn/         # Componente C9 (rg_gcn)
│   │       ├── gpuvm.cpp
│   │       └── ip_blocks.cpp
│   ├── shim/            # Componente C3 (libdrm_haiku)
│   │   ├── radeon_shim.c
│   │   └── amdgpu_shim.c
│   ├── asm/             # Componente C7 (rg_asm)
│   │   ├── x86/
│   │   ├── arm/
│   │   └── common/
│   └── lvp_hw/          # Componente C5 (puente Vulkan híbrido)
│       ├── lvp_planner.cpp
│       └── lvp_exec_cpu.cpp
└── scripts/
    └── gen_asic_db.py   # Generador de la base de datos de hardware

________________


12. Referencias y fuentes primarias
1. Documentación Oficial de AMD (PDFs de arquitectura):
   * R600/R700 3D Register Reference
   * Evergreen Family Display and 3D Register Reference
   * Southern Islands Series Instruction Set Architecture
   * RDNA / RDNA2 / RDNA3 Instruction Set Architecture
2. Código Fuente de Mesa 3D:
   * src/gallium/drivers/r300/
   * src/gallium/drivers/r600/
   * src/gallium/drivers/radeonsi/
   * src/amd/vulkan/ (RADV)
3. Driver DRM del Kernel de Linux:
   * drivers/gpu/drm/radeon/
   * drivers/gpu/drm/amd/amdgpu/
4. Repositorios y discusiones de la comunidad Haiku:
   * Trabajo del desarrollador X512 sobre RadeonGfx y puerto de Vulkan.
   * Archivos del kernel de Haiku: headers/os/drivers/KernelExport.h.
   * Archivos del kernel de Haiku: headers/os/drivers/KernelExport.h.
Apéndice A — Pseudocódigo del verificador (función verify(client, ib, relocs))
int verify(struct rg_client *client, struct rg_ib *ib, struct rg_reloc *relocs, uint32_t n_relocs) {
    uint32_t i = 0;
    while (i < ib->dwords) {
        uint32_t header = ib->ptr[i];
        uint32_t type = (header >> 30) & 0x3;
        
        if (type == 3) { // Paquete PM4 Type-3
            uint32_t opcode = (header >> 8) & 0xFF;
            uint32_t count = ((header >> 16) & 0x3FFF) + 1;
            
            if (i + count > ib->dwords) return -EINVAL; // IB Truncado
            
            switch (opcode) {
                case IT_SET_CONFIG_REG:
                case IT_SET_CONTEXT_REG: {
                    uint32_t reg = ib->ptr[i + 1];
                    if (!rg_is_reg_allowed(reg)) return -EPERM;
                    if (rg_reg_needs_reloc(reg)) {
                        struct rg_bo *bo = rg_find_bo(client, ib->ptr[i + 2]);
                        if (!bo) return -EACCES;
                        ib->patched_ptr[i + 2] = bo->gpu_addr + ib->ptr[i + 2];
                    }
                    break;
                }
                case IT_NOP:
                    break;
                default:
                    if (!rg_is_opcode_allowed(opcode)) return -EPERM;
            }
            i += count + 1;
        } else {
            i++;
        }
    }
    return 0;
}
Apéndice B — Checklist de la primera sesión del agente
11. Paso 1: Inicializar docs/PROGRESS.md y docs/DECISIONS.md.
12. Paso 2: Auditar licencias de las fuentes referenciadas (§0).
13. Paso 3: Extender radeon_hd en el árbol de Haiku y documentar en REGISTER_OWNERSHIP.md.
14. Paso 4: Compilar y probar los ioctls del helper en hardware real [HUMANO].
15. Paso 5: Implementar el esqueleto del servidor radeon_gpud y el protocolo IPC.
16. Paso 6: Construir el verificador CS y la suite de fuzzing con ASan.
17. Paso 7: Implementar la abstracción de firmware y anillo CP para R600.
18. Paso 8: Construir el shim libdrm_haiku y conectarlo con Mesa r600g.
19. Paso 9: Ejecutar pruebas 3D básicas en R600 [HUMANO].
20. Paso 10: Registrar resultados y actualizar el estado en docs/PROGRESS.md.

## 13. Fases Faltantes Detalladas (A4, Fase 9, Fase 10 y Pista G G0–G9)

### 13.1 Fase A4 — Integración en lvp-hw y Medición de Rendimiento
* **Trabajo:** Conexión de los kernels SIMD optimizados (`rg_k6_copy_nt`, `rg_k8_memset128`, etc.) con el ejecutor CPU del puente híbrido `lvp-hw` (`lvp_exec_cpu.cpp`). Validación del oráculo CPU (`RG_VK_MODE=cpu-only`).
* **CA:** Las transferencias de staging y limpieza de buffers híbridos ejecutan los caminos SIMD con despacho dinámico sin corrupción visual.
* **Prueba:** Suite de comparación diferencial en `tests/test_main.cpp` entre el renderizado híbrido GPU+CPU y el oráculo CPU puro.

### 13.2 Fase 9 — WSI (Window System Integration) y Presentación Haiku
* **Trabajo:** Implementación de `VK_KHR_surface` y la extensión nativa `VK_HAIKU_bmessenger_surface`. Creación de la capa de presentación que copia el BO de renderizado de la GPU al `BBitmap` / área compartida del `app_server` de Haiku mediante blit optimizado.
* **CA:** Las aplicaciones Vulkan / OpenGL en Haiku pueden presentar fotogramas en una ventana estándar de Haiku sin bloqueos ni fugas de áreas.
* **Prueba:** Ejecución de `vkcube` o test de superficie WSI conectado a un `BWindow` [HUMANO].

### 13.3 Fase 10 — Integración con app_server, Aceleración 2D y Tracker Add-on
* **Trabajo:** 
  1. Conexión de los hooks de aceleración 2D del accelerant (`fill_rectangle`, `screen_to_screen_blit`) gestionados por `radeon_hd` con el motor de dibujo del `app_server`.
  2. Integración con el **Tracker** y la **Deskbar** de Haiku mediante un Replicant / Deskbar Add-on y soporte acelerado para renderizado de miniaturas (thumbnails) de imágenes/texturas pesadas en carpetas de BFS usando los kernels de conversión de formato.
* **CA:** El escritorio de Haiku (Tracker/Deskbar) opera fluido con aceleración 2D de rectángulos y blits, y el Deskbar muestra estadísticas o estado de la GPU.
* **Prueba:** Despliegue del replicant de monitorización en el Deskbar y navegación de carpetas con miniaturas en el Tracker [HUMANO].

### 13.4 Pista G (Desglose Detallado G0 a G9)
* **G0 Enumerador:** Base de datos ASIC generada automáticamente desde Mesa/libdrm (`scripts/gen_asic_db.py`). Clasificación del 100% de IDs PCI de AMD.
* **G1 Evaluación:** Informe formal registrado en `docs/RADEONGFX_EVAL.md` sobre el trabajo previo de X512 y selección de la arquitectura unificada MIT.
* **G2 Polaris (GFX8):** Ciclo de vida de bloques IP (Common/NBIO → GMC → IH → PSP → SMU → GFX → SDMA), carga de firmware y anillos GFX/SDMA con aislamiento GPUVM [HUMANO].
* **G3 Shim amdgpu:** Emulación de `amdgpu_drm.h`, ioctls de sincronización por timeline (`SYNCOBJ_TIMELINE_*`) y `AMDGPU_INFO_DEV_INFO`.
* **G4 radeonsi:** Integración de Mesa con el driver OpenGL `radeonsi` sobre `libdrm_amdgpu` en Haiku [HUMANO].
* **G5 RADV:** Soporte de Vulkan 1.4 nativo en arquitecturas GCN4+ / RDNA, superando las suites dEQP-VK [HUMANO].
* **G6 Vega y APUs (GFX9):** Handshake con el procesador de seguridad PSP y gestión de memoria GFX9 [HUMANO].
* **G7 RDNA 1–4:** Descubrimiento dinámico de IP mediante tablas en VRAM (`amdgpu_discovery`), anillos MEC y firmware PSP v12-v14 [HUMANO].
* **G8 SI/CIK (Opcional):** Compatibilidad con GFX6/GFX7 y microcódigo MC [HUMANO].
* **G9 Energía (Opcional):** Mensajería SMU y control DPM básico para medición y ajuste de frecuencias [HUMANO].
