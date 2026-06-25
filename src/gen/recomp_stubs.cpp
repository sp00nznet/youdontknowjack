/* Stubs for func_X symbols the lifted code references but never defines.
 *
 * These are fall-through / branch edges that point just past a clipped
 * function boundary into non-code (here: 0x483108, the address right after
 * func_00482200's terminating blr — the start of the .rodata that used to be
 * mis-lifted as a 158k-instruction "function"). They are never actually
 * reached at runtime (the predecessor returns first); routing them to the
 * unlifted-stub logger just lets the image link and reports if one is ever hit.
 *
 * Regenerate after a re-lift:
 *   comm -23 <(declared funcs in ppu_recomp.h) <(defined funcs in *.cpp)
 */
#include "ppu_recomp.h"
#include <cstdint>

extern "C" void ppu_unlifted_stub(uint64_t addr, ppu_context* ctx);

void func_00483108(ppu_context* ctx) { ppu_unlifted_stub(0x483108ULL, ctx); }
