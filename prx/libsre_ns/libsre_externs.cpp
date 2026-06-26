/* No-op definitions for libsre call targets that find_functions did not
 * detect as function starts. All fall in libsre's self-relocation stub
 * region (~0x1D71E-0x1DB02); since the PRX image is PRE-relocated by
 * tools/prx_relocate.py, re-running that code is unnecessary and a no-op
 * is the correct behavior. Regenerate if the libsre lift changes. */
#include "ppu_recomp.h"

void libsre_func_3001DAF8(ppu_context* ctx) { (void)ctx; }
