# Decisiones de Arquitectura (ADR) - Haiku Radeon Legacy

Registro de decisiones y desviaciones. Cada ADR indica si está aprobada o
pendiente de validación en hardware.

- **ADR-001** (Aprobado): Servidor GPU en userland con helper de kernel
  mínimo.
- **ADR-002** (Aprobado): Contrato con Mesa basado en la ABI `radeon_drm.h`
  mediante un shim IPC.
- **ADR-003** (Aprobado): Verificador de command streams en userland,
  deny-by-default, con copia anti-TOCTOU del IB y parcheo de relocs.
- **ADR-006** (Aprobado): Ejecución híbrida GPU+CPU para cubrir todo el
  espectro Radeon antiguo.
- **ADR-007** (Aprobado): Kernels en ensamblador con referencia C y
  validación diferencial obligatoria.
- **ADR-008** (Aprobado): Éxito IPC = tamaño de la respuesta (>0); los
  errores son códigos `RG_*` negativos. El `status` dentro del payload
  replica el resultado y debe inicializarse siempre.
- **ADR-009** (Aprobado): El campo de longitud PM4 (bits 16..29) es el total
  de dwords **menos uno, incluyendo la cabecera**. Tipo 0 y tipo 3 consumen
  `total` dwords; en `SET_*_REG` hay `(total-1)/2` pares.
- **ADR-010** (Aprobado): Unidades de los kernels asm: bloque = 16 bytes
  (NEON) o 32 bytes (AVX2). El despachador deriva cantidad de bytes/píxeles
  de esa convención; un desajuste provoca colas sin convertir (bug K1 ya
  corregido).
- **ADR-011** (Aprobado): Sin firmware de CP no hay aceleración. El arranque
  legacy aborta en el paso `firmware` con `RG_ENOENT`; nunca se habilita el
  anillo "a ciegas".
- **ADR-012** (Aprobado): El fallback de memoria es VRAM → GTT → CPU cuando
  los dominios lo permiten. Una asignación VRAM que cabe exactamente se
  queda en VRAM (no se degrada por quedar llena).
- **ADR-013** (Aprobado): GPUVM replica la semántica de direcciones virtuales
  del cliente: mismo VA puede mapear a PA distinto por VMID; permisos
  READ/WRITE/EXEC se comprueban en `translate`.
- **ADR-014** (Aprobado): `rg_asic_lookup` devuelve `rg_asic_unknown` para
  IDs no listados; ese descriptor tiene familia `UNKNOWN` y el planificador
  lo envía a CPU.
- **ADR-015** (Pendiente): Números de registro/opcode de `rg_policy_test` y
  layouts de detile son provisionales; deben generarse desde
  `r300_reg.h`/`r600d.h`/`evergreend.h` y los PDFs de AMD antes de
  producción.
- **ADR-016** (Aprobado): El helper de producción publica
  `graphics/radeon_hd` y se integra en el driver nativo existente
  (ADR-004). Un build aislado usa `RG_STANDALONE=1` y publica
  `graphics/radeon_legacy` para no colisionar. `RG_HELPER_DEVICE_NAME`
  sigue permitiendo override explícito.
- **ADR-017** (Aprobado): La evicción VRAM→GTT es LRU sobre BOs idle y debe
  preservar contenido. En host se conserva el `host_ptr`; en hardware la
  copia y el flush de GART/TLB son `[HUMANO]`. `GartTable` modela el mapa
  pero aún no programa hardware.

## Desviaciones respecto al plan

1. **K1 (swizzle)**: el plan asumía "bloques de 16/32 píxeles"; la
   implementación usa bloques de bytes acordes a la instrucción
   (`tbl`/`vpshufb`). Documentado en ADR-010.
2. **Firmware en host**: ausente por diseño; el selftest lo reporta como
   `[warn]` sin contar como fallo, porque el arranque falla cerrado
   (ADR-011).
3. **Tabla ASIC**: semilla de 17 entradas mantenida a mano; el generador
   (`scripts/gen_asic_db.py`) puede importar IDs de Mesa y conserva la
   semilla para no perder GPUs legacy. La cabecera PCI deduplica IDs
   (17 entradas → 16 IDs, por `0x67DF` repetido).
4. **Helper integrado**: el nombre por defecto pasó de `radeon_legacy` a
   `radeon_hd` para cumplir la integración nativa; el módulo aislado se
   conserva para desarrollo (ADR-016).
