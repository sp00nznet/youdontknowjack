#include "ppu_recomp.h"
#include <cstdint>
extern "C" void ppu_unlifted_stub(uint64_t addr, ppu_context* ctx);
void func_FFFFF1A4(ppu_context* ctx) { ppu_unlifted_stub(0xFFFFF1A4ULL, ctx); }
/* Newer lifter marks 0x483108 external (cold conditional tail of func_00482200,
 * in the un-lifted region past the last function). Stub via the unlifted handler. */
void func_00483108(ppu_context* ctx) { ppu_unlifted_stub(0x00483108ULL, ctx); }
