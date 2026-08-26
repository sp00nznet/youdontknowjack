/* YDKJ real-PRX loader: brings the lifted libsre (Sony cellSpurs/cellSync) into
 * guest RAM at boot so the title's SPURS imports dispatch into REAL recompiled
 * Sony code instead of HLE stubs. Defines the weak ps3_load_prx_modules() hook
 * that the generic boot harness (runtime/ppu/tests/boot_main.cpp) calls after the
 * lifted function table is registered and vm_base is live.
 *
 * Reuses the lifted libsre from the flОw sister-project (prx/libsre_ns/*,
 * prx/libsre.linked.bin) -- firmware-version-close; covers 66/82 of YDKJ's SPURS
 * NIDs (the other 16 fall back to HLE stubs). The PRX toolchain + loader are in
 * the shared ps3recomp runtime (runtime/prx/prx_loader.*). */
#include "prx_loader.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern "C" {
/* libsre's namespaced lifted tables (prx/libsre_ns/*). The lifter's func_entry
 * is binary-compatible with prx_func_entry. */
extern const prx_func_entry libsre_function_table[];
extern const uint64_t        libsre_function_table_count;
extern const prx_export      libsre_exports[];
extern const uint32_t        libsre_export_count;

/* YDKJ's indirect-dispatch registrar (runtime/ppu/ppu_loader.cpp). extern "C"
 * matches by name; the (uint64_t, fn) ABI is identical to ppu_register_function. */
void ppu_register_function(uint64_t addr, void (*fn)(void*));

/* HLE registration + sysPrxForUser CRT shims libsre imports but the title does
 * not (so the generated NID table omits them). Registered here so cellSpurs'
 * init path (which strcpy/strncat's the SPU thread-group name and validates the
 * returned dst pointer) resolves instead of getting a 0 from the unresolved
 * fallback. */
void ps3_hle_register(unsigned int nid, const char* name, void* handler);
char* _sys_strcpy(char* dst, const char* src);
char* _sys_strncat(char* dst, const char* src, unsigned int size);
int   _sys_strncmp(const char* s1, const char* s2, unsigned int size);
}


/* ---- [TASKPROBE] real SPURS/taskset capture -----------------------------
 *
 * The LLE libsre path bypasses the HLE cellSpurs setters, so the runtime SPU
 * taskset-policy handoff (runtime/spu/spu_workload.c) never learns the real
 * CellSpurs and taskset EAs -- it would fall back to a hardcoded guess.
 *
 * This used to be hand-patched into the generated prx/libsre_ns/ppu_recomp.c,
 * so every re-lift silently dropped it and the guess came back. Wrapping the
 * two taskset-create entries at registration time keeps it in tracked source:
 * re-lift as often as you like, the probe survives. Registration overwrites by
 * address (ppu_register_function), so these must run AFTER prx_load_module. */
#include "ppu_context.h"

extern "C" {
/* defined C-side in ps3recomp libs/spurs/cellSpurs.c */
extern uint32_t g_ydkj_real_taskset_ea, g_ydkj_real_spurs_ea;
}

/* NO extern "C" here: the generated header declares the lifted functions
 * OUTSIDE its extern "C" block, and CMake compiles the PRX modules as C++, so
 * they carry C++ linkage. Declaring them extern "C" mangles differently and
 * silently fails to resolve at link time. */
void libsre_func_30014CF8(ppu_context* ctx);   /* cellSpursCreateTasksetWithAttribute */
void libsre_func_30014DC4(ppu_context* ctx);   /* cellSpursCreateTaskset */

/* r3 = CellSpurs*, r4 = CellSpursTaskset* (both create entries share the ABI). */
static void ydkj_taskprobe(ppu_context* ctx, const char* what)
{
    g_ydkj_real_spurs_ea   = (uint32_t)ctx->gpr[3];
    g_ydkj_real_taskset_ea = (uint32_t)ctx->gpr[4];
    fprintf(stderr, "[TASKPROBE] libsre %s spurs=0x%08X taskset=0x%08X\n",
            what, g_ydkj_real_spurs_ea, g_ydkj_real_taskset_ea);
    fflush(stderr);
}

static void ydkj_probe_CreateTasksetWithAttr(void* c)
{
    ppu_context* ctx = (ppu_context*)c;
    ydkj_taskprobe(ctx, "CreateTasksetWithAttr");
    libsre_func_30014CF8(ctx);
}

static void ydkj_probe_CreateTaskset(void* c)
{
    ppu_context* ctx = (ppu_context*)c;
    ydkj_taskprobe(ctx, "CreateTaskset");
    libsre_func_30014DC4(ctx);
}

static int ydkj_spu_image_close_stub(void) { return 0; } /* sys_spu_image_close */

/* Adapter to the prx_register_fn signature the loader expects. */
static void ydkj_prx_register(uint32_t addr, void (*host)(void*))
{
    ppu_register_function((uint64_t)addr, host);
}

extern "C" void ps3_load_prx_modules(void)
{
    /* Proven path (flОw) but under active integration: libsre's lifted code still
     * has TOC/GOT-relocation gaps + unresolved mid-function jump-table targets
     * that leave SPURS construction incomplete, so it currently gets the title
     * stuck earlier than the HLE-stub path. Gate behind YDKJ_LIBSRE=1 so the
     * default build keeps the stub behaviour until the libsre execution is fixed.
     * Set YDKJ_LIBSRE=1 to dispatch cellSpurs into real recompiled Sony code. */
    { const char* e = getenv("YDKJ_LIBSRE"); if (!(e && e[0]=='1')) {
        fprintf(stderr, "[init] libsre disabled (set YDKJ_LIBSRE=1 to load real cellSpurs)\n");
        return; } }

    /* libsre.linked.bin lives in the project's prx/ (cwd is the project dir when
     * launched as ./build/ydkj_boot.exe input/EBOOT.elf). */
    const char* candidates[] = {
        "prx/libsre.linked.bin",
        "../prx/libsre.linked.bin",
        "D:/recomp/ps3games/youdontknowjack/project/prx/libsre.linked.bin",
    };
    uint8_t* img = 0; uint32_t sz = 0;
    for (unsigned i = 0; i < sizeof(candidates)/sizeof(candidates[0]); i++) {
        img = prx_image_load_file(candidates[i], &sz);
        if (img) { fprintf(stderr, "[init] libsre image: %s (%u bytes)\n", candidates[i], sz); break; }
    }
    if (!img) {
        fprintf(stderr, "[init] libsre.linked.bin not found -- cellSpurs uses HLE stubs\n");
        return;
    }

    prx_module m;
    memset(&m, 0, sizeof(m));
    m.name         = "libsre";
    m.base         = 0x30000000;   /* must match the relocate/lift base */
    m.image        = img;
    m.image_size   = sz;
    m.funcs        = libsre_function_table;
    m.func_count   = libsre_function_table_count;
    m.exports      = libsre_exports;
    m.export_count = libsre_export_count;

    prx_load_result r = prx_load_module(&m, ydkj_prx_register);
    fprintf(stderr, "[init] libsre load %s: %u funcs registered, %u exports in registry\n",
            r.ok ? "OK" : "FAILED", r.funcs_registered, prx_export_registry_count());
    free(img);

    /* Override the two taskset-create entries with the probes (after the bulk
     * registration above, so the wrappers win the address). */
    ppu_register_function(0x30014CF8ull, ydkj_probe_CreateTasksetWithAttr);
    ppu_register_function(0x30014DC4ull, ydkj_probe_CreateTaskset);

    /* Supplemental sysPrxForUser CRT shims for libsre's imports. */
    ps3_hle_register(0x99C88692u, "_sys_strcpy",  (void*)_sys_strcpy);
    ps3_hle_register(0x996F7CF8u, "_sys_strncat", (void*)_sys_strncat);
    ps3_hle_register(0x04E83D2Cu, "_sys_strncmp", (void*)_sys_strncmp);
    ps3_hle_register(0xE0DA8EFDu, "sys_spu_image_close", (void*)ydkj_spu_image_close_stub);
}
