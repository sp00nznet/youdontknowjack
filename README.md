# You Don't Know Jack — Static Recompilation

> Turning the PS3 disc binary of Jellyvision's irreverent trivia classic into a native PC executable — no emulator required.

**You Don't Know Jack** (2011, THQ / Jellyvision) is the high-definition revival of the snarkiest quiz show in video games. Dis-or-Dat, the Jack Attack, the host's relentless sarcasm, and a fake-commercial sponsor break every other question — all driven by a Scaleform Flash UI and a *mountain* of pre-recorded audio. It shipped on PS3, Xbox 360, Wii, and PC… but the PS3 version, like most PS3 games, has never run anywhere but a PS3 or RPCS3.

This project takes the decrypted PS3 `EBOOT.elf`, lifts every PowerPC function to C/C++, and links it against [**ps3recomp**](https://github.com/sp00nznet/ps3recomp) — a set of HLE runtime libraries that replace the Cell/LV2 operating system with native host implementations. The goal: a standalone Windows executable that boots Jack straight to the host's barbs, no emulator in sight.

> **You bring your own legally-dumped disc.** No game binaries, audio, or trivia content are included in this repository — only analysis metadata and the recompilation harness.

---

## Why this is a *great* recomp target

Your average AAA PS3 game is a Cell-saturated nightmare of SPU compute and bespoke renderers. You Don't Know Jack is the opposite. Under the hood it's almost entirely **menus, a Flash UI, and audio** — exactly the profile that static recompilation eats for breakfast:

- **One self-contained 5.3 MB EBOOT.** No separate `.sprx` modules to relocate and stitch in.
- **5,859 functions.** Tiny next to a PhyreEngine title (flOw weighed in at 100k+).
- **265 imports across 23 libraries — and ps3recomp already implements 19 of them.** The four it doesn't are all *optional online/social* features (matchmaking, PSN store, voice chat) that an offline single-player port can simply stub out.
- The 2 GB of disc data is **601 trivia/"prize" content packs, FMOD `.fsb` audio banks, and Scaleform `.gfx` flash** — data, not code.

---

## Current Status

| Metric | Value |
|---|---|
| Title | YOU DON'T KNOW JACK |
| Title ID | **BLUS30569** (USA, disc) |
| Engine | Custom (Scaleform Flash UI + FMOD audio) |
| Binary | `EBOOT.elf` — 5.2 MB, ELF64 big-endian PowerPC64, `ET_EXEC` |
| Entry | OPD `0x5309e8` → code `0x5ecf4`, TOC `0x544370` |
| Functions detected | **5,859** (6,630 OPD descriptors) |
| Functions lifted to C | **14,380** (5,859 base + 1,467 jump-table cases + 7,054 mid-function tail-entry wrappers) |
| Generated source | **19 chunks, 656 MB** *(was 5.2 GB before the .rodata fix — see note)* |
| Unique call targets | 6,104 |
| Imported libraries | **23** |
| Imported functions | **265** |
| Module coverage | **19 / 23** libraries already implemented in ps3recomp |
| SPU usage | SPURS framework only (management-level; no raw SPU compute observed yet) |
| Target | Windows x86-64 (Linux planned) |

### Phase Progress

| Phase | Status | Notes |
|---|---|---|
| Disc inventory | ✅ **Complete** | 2,709 files — 1 EBOOT, 601 `prize_*.zip` packs, `.fsb` audio banks, Scaleform `.gfx` |
| SELF decryption | ✅ **Complete** | `EBOOT.BIN` → `EBOOT.elf` via `ps3sce` (retail key rev 7, APP SELF) |
| ELF structural analysis | ✅ **Complete** | 2 PT_LOAD segments, OPD/TOC resolved, entry point located |
| Function boundary detection | ✅ **Complete** | 5,859 functions seeded from the `.opd` descriptor table |
| Import / NID extraction | ✅ **Complete** | 265 NIDs across 23 libraries catalogued |
| Module coverage triage | ✅ **Complete** | 19/23 ready; 4 to stub (see below) |
| PPU lifting (→ C/C++) | ✅ **Complete** | 14,380 functions → 19 C++ chunks (656 MB) via `ppu_lifter.py` |
| Lifter `.rodata` fix | ✅ **Complete** | new `--code-end` bound; killed a 5.2 GB → 656 MB source explosion |
| HLE NID table | ✅ **Complete** | 357 handlers / 18 modules (`src/gen/ppu_hle_nids.cpp`) |
| Boot harness (CMake) | ✅ **Complete** | clang-cl, links prebuilt `ps3recomp_runtime.lib` |
| Build & link | ✅ **Complete** | all 26 objects compile; **`ydkj_boot.exe` (64 MB) links** (1 fall-through stub) |
| First boot | ✅ **Runs real code** | entry dispatched, TLS up, **170 MB heap allocated** |
| Import-stub → HLE wiring | ✅ **Solved** | new lifter `--hle-stubs`: all 265 stubs → `ps3_hle_call(nid)`. **Cleared the `0x39800000` wall** (where gunstar is stuck) |
| Thread creation | ✅ **Wired** | `sys_ppu_thread_create` NID → real lv2 thread spawn + generic **thread-entry trampoline**; the `"AsyncLoad"` worker runs |
| TOC-save on imports | ✅ **Fixed** | save r2 to `0x28(r1)` per PPC64 ABI — **eliminated all the static-init corruption** (the vtable sweep + 40 OOB) |
| `bcctrl` mistranslation | ✅ **Fixed** | 5,596 vtable calls were jumping to a garbage address; now dispatch via `ps3_indirect_call` |
| Thread / main stacks | ✅ **Fixed** | VM grown to cover the `0xD0000000` stack region; 256 MB host stacks |
| HLE pointer bridge | ⏳ **Next** | HLE funcs that take pointer out-params deref guest addresses as host pointers (`cellGameBootCheck` fixed; the rest are the next pass) |
| CRT startup | ⬜ Not started | TLS → mutexes → malloc → static ctors |
| Game `main()` / module load | ⬜ Not started | |
| Scaleform UI bring-up | ⬜ Not started | the "menus" half of the game |
| Audio (FMOD `.fsb` → cellAudio) | ⬜ Not started | the "audio" half of the game |
| First playable question | ⬜ Not started | 🎯 the real goal |

> **🔬 War story: the 5.2 GB lift, and the one-line ISA detail that caused it.**
> The first full lift produced **5.2 GB** of C++ (≈600k lines/chunk) — absurd for a
> 5 MB binary (flОw's *100k+* functions were ~156 MB). The hunt:
>
> 1. Found one mis-detected 634 KB "function" that ran to the end of the code
>    segment — but clipping it changed *nothing*.
> 2. Found **1,617 garbage functions at `.rodata` addresses** (`func_005xxxxx`),
>    each lifting tens of thousands of `/* TODO: .word */` junk "instructions."
> 3. Root cause: PS3 EBOOTs pack **`.rodata` into the same R-X segment as `.text`**.
>    A `bc`-form (conditional branch) *data word* whose 16-bit immediate happens to
>    point into that rodata was promoted by the lifter to a `func_X` jump target;
>    the mid-function pass then re-emitted it to the next boundary — a quadratic
>    blowup seeded by **random data that merely decodes as a branch**.
>
> **Fix:** a new opt-in `--code-end` bound in `ppu_lifter.py`. The ELF section
> headers say executable code ends at `0x4874E0`; any branch/call/jump-table
> target at or beyond that is treated as data, not a function. Result:
> **5.2 GB → 656 MB**, 131 → 19 chunks, 0 bogus rodata functions. The remaining
> `.word`/`vmx` TODOs are genuine undecoded AltiVec/VMX SIMD in *real* functions
> (the same class flОw carries) — implemented as the boot progresses, not bloat.

---

## First Boot

`ydkj_boot.exe` loads the decrypted EBOOT, registers the 14,380 lifted functions
and 357 HLE handlers, and dispatches the real entry point. It gets impressively far:

```
[ppu] loaded 2 PT_LOAD segments, entry OPD 0x005309E8
[ppu] TLS image 0x10F00000 (r13/TP=0x10F07000)
[ppu] run: code 0x0005ECF4, toc 0x00544370, sp 0x0FF00000
[sys_memory] allocate(size=0xAA00000, flags=0x400)   ← game requests its 170 MB main heap
[sys_memory] allocate -> 0x40000000                  ← granted
[ppu] unresolved indirect call -> 0x39800000  (r12=0x00470870, the real target)
[ppu] lv2_syscall 988 (stub) × N
[ppu] FATAL: stuck calling 0x39800000 (2000 times) -- aborting run
```

The first build stalled on `bctrl → 0x39800000` (the bytes of `li r12,0`) — the
**firmware-import-stub wall** that also blocks Gunstar. The cause: the lifter
emitted the *literal* `.lib.stub` trampolines, which dereference an import pointer
table the recomp never fills. The fix is a new generic lifter flag **`--hle-stubs`**
that rewrites each of the 265 import stubs to `ps3_hle_call(nid, ctx)` — dispatching
straight to the registered HLE handler. That **cleared the wall**:

```
[crt] sys_initialize_tls: block 0x0E000000, r13=0x0E007000   ← TLS init runs
[sys_memory] allocate(0xAA00000) -> 0x40000000               ← 170 MB heap
[hle] unresolved NID 0x24A1EA07                              ← = sys_ppu_thread_create
[ppu] unresolved indirect call -> 0x40000030 .. 0x40000120   ← static-init vtable sweep
```

So the recompiled CRT now runs **much deeper** — TLS, heap, and into thread
creation / C++ static constructors. The next blocker is identified precisely:
`sys_ppu_thread_create` (NID `0x24A1EA07`) is imported via `sysPrxForUser` but
ps3recomp implements thread creation as a *syscall*, so the NID is unwired; the
CRT's thread/static-init then walks an uninitialized object table at the heap
base and calls heap addresses as function pointers. Bridging the `sys_ppu_thread_*`
NIDs is the next step. Full trace: [`docs/BOOT_TRACE.txt`](docs/BOOT_TRACE.txt).

## Module Coverage

Of the 23 firmware libraries the game imports, ps3recomp already ships real host implementations for **19**. The remaining four are all online/social and can be stubbed for an offline port.

| Library | NIDs | ps3recomp status |
|---|---:|---|
| `sysPrxForUser` | 26 | ✅ Complete — threads, lwmutex/lwcond, heap, libc |
| `cellGcmSys` | 22 | ✅ Complete — RSX command buffer + state |
| `sys_fs` | 18 | ✅ Complete — file I/O w/ host path mapping |
| `cellRudp` | 17 | ✅ Complete — reliable UDP |
| `cellSysutil` | 17 | ✅ Complete — callbacks, system params, BGM |
| `cellSpurs` | 15 | 🟡 Partial — management only (no SPU execution) |
| `sys_net` | 14 | ✅ Complete — BSD sockets |
| `cellAudio` | 10 | ✅ Complete — WASAPI / SDL2 mixing |
| `sceNpTrophy` | 9 | ✅ Complete — persistent trophy storage |
| `cellHttp` | 6 | ✅ Complete — HTTP/1.1 |
| `sys_io` | 6 | 🟡 Stub — low-level pad I/O syscalls |
| `cellGame` | 5 | ✅ Complete — boot/content dirs |
| `cellNetCtl` | 5 | ✅ Complete — network control |
| `sceNp` | 27 | ✅ Complete — NP core |
| `cellSync` | 3 | ✅ Complete — sync primitives |
| `cellSysmodule` | 3 | ✅ Complete — module tracking |
| `cellSsl` | 3 | ✅ Complete — SSL/TLS |
| `cellRtc` | 2 | ✅ Complete — real-time clock |
| `cellUserInfo` | 2 | ✅ Complete — default user |
| `cellSaveData` | 1 | ✅ Complete — save/load |
| `sceNp2` | 28 | ⛔ **TODO** — NP2 matchmaking/rooms (stub for offline) |
| `sceNpCommerce2` | 16 | ⛔ **TODO** — PSN store / DLC (stub for offline) |
| `cellSysutilAvc2` | 10 | ⛔ **TODO** — audio/video (voice) chat (stub for offline) |

---

## How It Works

```
EBOOT.BIN  ──ps3sce──▶  EBOOT.elf  ──ppu_loader──▶  meta/*.json
 (SELF, encrypted)     (PPC64 BE)      │  (image, OPD/TOC, functions, imports)
                                       ▼
                                  ppu_lifter  ──▶  src/recomp/*.cpp   (lifted PPC → C)
                                       │
                                       ▼
            ps3recomp runtime  +  generated HLE NID table  ──CMake/clang-cl──▶  ydkj.exe
            (LV2 syscalls, cellXxx HLE, vm, big-endian memory)
```

`ps3recomp` does the heavy lifting on the runtime side; this repo is the **per-game project**: the decryption recipe, the analysis metadata, the module configuration, and (soon) the lifted code + boot harness.

## Repository Layout

```
project/
├── config.toml          # module map + loader/lift constants for this title
├── CMakeLists.txt       # clang-cl boot harness (links ps3recomp_runtime.lib)
├── meta/                # analysis output (committed — pure metadata, no game code)
│   ├── EBOOT.functions.json   # 5,859 function boundaries (rodata-clipped)
│   ├── EBOOT.functions.raw.json # raw detector output, for provenance
│   ├── EBOOT.imports.json     # 265 NIDs × 23 libraries
│   ├── EBOOT.image.json       # segment manifest
│   ├── EBOOT.loader.json      # entry / OPD / TOC
│   └── elf_analysis.json      # ELF headers / segments / sections
├── src/
│   ├── gen/             # generated HLE NID table (committed)
│   ├── compat/          # Win32 <dirent.h>/<unistd.h> shims (committed)
│   └── recomp/          # 656 MB of lifted PPU→C (generated; gitignored)
└── input/               # YOUR decrypted EBOOT goes here (gitignored)
```

## Reproducing the analysis

```bash
# 1. Extract the EBOOT from your own disc dump
7z e "You Don't Know Jack (USA).7z" "...\PS3_GAME\USRDIR\EBOOT.BIN" -oinput/

# 2. Decrypt the retail SELF → ELF
ps3sce -d input/EBOOT.BIN input/EBOOT.elf

# 3. Load: image + OPD/TOC + function table + imports
python <ps3recomp>/tools/ppu_loader.py input/EBOOT.elf --output meta/

# 4. Lift PPU -> C, bounding targets to .text so .rodata doesn't explode
python <ps3recomp>/tools/ppu_lifter.py input/EBOOT.elf \
    --functions meta/EBOOT.functions.json --output src/recomp \
    --header-name ppu_recomp.h --source-name ppu_recomp \
    --jobs 1 --code-end 0x4874E0 --hle-stubs meta/EBOOT.imports.json

# 5. Generate the HLE NID dispatch table for the imported libraries
python <ps3recomp>/tools/gen_hle_nids.py cellAudio cellGcmSys ... \
    --out src/gen/ppu_hle_nids.cpp

# 6. Build the boot harness (clang-cl, links the prebuilt runtime lib)
cmake -S . -B build -G Ninja -DCMAKE_C_COMPILER=clang-cl -DCMAKE_CXX_COMPILER=clang-cl
cmake --build build
./build/ydkj_boot input/EBOOT.elf
```

---

## Credits & Legal

Built on [**ps3recomp**](https://github.com/sp00nznet/ps3recomp), in the lineage of
[N64Recomp](https://github.com/N64Recomp/N64Recomp), [UnleashedRecomp](https://github.com/hedge-dev/UnleashedRecomp), and friends.

*You Don't Know Jack* is © Jellyvision / THQ. This project contains **no copyrighted game
code, assets, or audio** — only analysis metadata derived from a binary you must legally
own. It exists for preservation, interoperability, and the sheer joy of making a Cell
processor's worst nightmare run on a laptop. Bring your own disc.
