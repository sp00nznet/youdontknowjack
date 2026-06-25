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
| Functions lifted to C | 0 — *next milestone* |
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
| PPU lifting (→ C/C++) | ⏳ **Next** | `ppu_lifter.py` over all 5,859 functions |
| Build & link vs ps3recomp | ⬜ Not started | clang-cl boot harness |
| CRT startup | ⬜ Not started | TLS → mutexes → malloc → static ctors |
| Game `main()` / module load | ⬜ Not started | |
| Scaleform UI bring-up | ⬜ Not started | the "menus" half of the game |
| Audio (FMOD `.fsb` → cellAudio) | ⬜ Not started | the "audio" half of the game |
| First playable question | ⬜ Not started | 🎯 the real goal |

---

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
├── config.toml          # module map + loader constants for this title
├── meta/                # analysis output (committed — pure metadata, no game code)
│   ├── EBOOT.functions.json   # 5,859 function boundaries {start,end,toc,opd}
│   ├── EBOOT.imports.json     # 265 NIDs × 23 libraries
│   ├── EBOOT.image.json       # segment manifest
│   ├── EBOOT.loader.json      # entry / OPD / TOC
│   └── elf_analysis.json      # ELF headers / segments / sections
├── input/               # YOUR decrypted EBOOT goes here (gitignored)
└── src/                 # lifted code + harness (generated; gitignored)
```

## Reproducing the analysis

```bash
# 1. Extract the EBOOT from your own disc dump
7z e "You Don't Know Jack (USA).7z" "...\PS3_GAME\USRDIR\EBOOT.BIN" -oinput/

# 2. Decrypt the retail SELF → ELF
ps3sce -d input/EBOOT.BIN input/EBOOT.elf

# 3. Load: image + OPD/TOC + function table + imports
python <ps3recomp>/tools/ppu_loader.py input/EBOOT.elf --output meta/
```

---

## Credits & Legal

Built on [**ps3recomp**](https://github.com/sp00nznet/ps3recomp), in the lineage of
[N64Recomp](https://github.com/N64Recomp/N64Recomp), [UnleashedRecomp](https://github.com/hedge-dev/UnleashedRecomp), and friends.

*You Don't Know Jack* is © Jellyvision / THQ. This project contains **no copyrighted game
code, assets, or audio** — only analysis metadata derived from a binary you must legally
own. It exists for preservation, interoperability, and the sheer joy of making a Cell
processor's worst nightmare run on a laptop. Bring your own disc.
