/* libsre external/undefined-symbol stubs.
 *
 * The 32 import trampolines (PLT at 0x3001D718-0x3001DAF8) are handled by the
 * lifter's --hle-stubs pass (prx/libsre.imports.json): each is emitted inline as
 * ps3_hle_call(nid) dispatching libsre's sysPrxForUser/cellLibprof imports into
 * the title HLE bridge, so they need no definition here. This unit only carries
 * stubs for any remaining lifter-discovered targets that fall outside the lifted
 * code range. */
#include "ppu_recomp.h"
#include <stdint.h>
