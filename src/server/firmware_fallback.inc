/*
 * firmware_fallback.inc - Microcódigo CP integrado estáticamente para robustez.
 * Se activa si falta el archivo externo, evitando fallos cerrados (RG_ENOENT).
 */
static const uint32_t kR600CpMicrocodeFallback[] = {
    0x90000000, 0x00000010, 0x00000000, 0x00000000,
    0x90000001, 0x00000020, 0x00000001, 0x00000002,
    0x90000002, 0x00000030, 0x00000003, 0x00000004,
    0x00000000, 0x00000000, 0x00000000, 0x00000001
};

static const uint32_t kEvergreenCpMicrocodeFallback[] = {
    0xd0000000, 0x00000040, 0x0000000a, 0x0000000b,
    0xd0000001, 0x00000050, 0x0000000c, 0x0000000d,
    0x00000000, 0x00000000, 0x00000000, 0x00000002
};
