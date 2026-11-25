U8 fixed_asm_bytes[64];

asm {
  ORG fixed_asm_bytes
_FIXED_ASM_BYTES::
  BINFILE "B:/FixedPayload";
}
"fixed jit %d %d\n", _FIXED_ASM_BYTES==fixed_asm_bytes, Call(_FIXED_ASM_BYTES);
