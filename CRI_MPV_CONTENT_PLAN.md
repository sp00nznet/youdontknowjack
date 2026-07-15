# YDKJ: plan to get visible game content (cri_mpv/SPURS bring-up)

---
## 2026-07-15 — BUILD SCAFFOLD (cri_mpv → SPU decode chain)

**What changed since the 2026-07-03 plan below:** the render pipeline now WORKS. The
game draws its Scaleform attract/legal screen (real `DRAW_ARRAYS`/`DRAW_INDEX_ARRAY`,
logos + text) — see `docs/ydkj-attract-screen.png`. Two lifter re-lifts got us here:
1. **Game re-lift** (fixed lifter) — killed the systemic reused-save-slot corruption
   (`ld rN,off(r1)` reload of a reused callee-save slot mis-emitted as `_cs_N`), which
   had broken `data.toc` loading. That cleared the "only CLEAR_SURFACE" wall + the abort.
2. **libsre re-lift** (fixed lifter) — libsre (Sony cellSpurs/cellSync PRX) was lifted
   in June with the old buggy lifter. Re-lifting it **fixed the segfault, killed the
   `_cellSpursIsLaunchedFromTuner` assertion, and eliminated the `0x80410910`
   `cellSpursEventFlagInitialize` failure** (the 2026-07-03 step-3/4 blockers).

### The chain (what must happen, end to end)
```
game intro-video request
  -> cri_mpv PPU layer (statically linked in the game, func_0002CE78 reporter etc.)
  -> cellSpursInitializeWithAttribute + cellSpursCreateTaskset          [SPURS bring-up]
  -> cellSpursCreateTask(cri task = SPU image 22, cri_mpvps3spurs.elf @0x4F5F80)
  -> PPU feeds decode cmds/data via the task's INBOUND MAILBOX
  -> SPU runs the REAL recompiled decoder (image 22), decodes a frame from an EA
  -> SPU posts completion: cellSpursEventFlagSet -> attached lv2 queue (q=1, key=0, sz=64)
  -> PPU thread-1 (blocked on event_queue_receive q=1 @cia=0x00530E38) wakes
  -> PPU reads decoded frame, issues GCM draws -> D3D12 shows the video
```

### Current gate (where it's stuck NOW, post-libsre-relift)
`cellSpursInitializeWithAttribute` returns **`0x80410882`**, then
`cellSpursCreateTaskset` returns **`0x80410911`** (STAT / invalid-state cascade).
The cri SPU task (image 22) is therefore **never created**; the main thread spins
pumping criFs waiting for decode that never comes. This is the load-bearing gate:
until `CreateTaskset` succeeds, nothing downstream can run.

### Ordered build steps (verify each before the next)

**Step 1 — SPURS instance valid so CreateTaskset succeeds.  [FOUNDATION, current gate]**
- Root the `0x80410882` from `cellSpursInitializeWithAttribute` (libsre). Options:
  (a) it's a remaining libsre issue — trace the libsre init fn's STAT check (what state
      field it validates on the CellSpurs @0x40009F00 instance and why it's wrong);
  (b) the reverted SPU WIP (spu_channels.c/spu_dma.h/spu_workload.h) held instance-
      population logic — re-do it PROPERLY (define `spu_policy_image_lookup`) rather
      than the discarded half-version.
- Instrument: register a wrapper on the libsre `cellSpursInitializeWithAttribute`
  export (or watch @0x40009F00 field writes) to see which check returns STAT.
- VERIFY: `[cellSpurs] CreateTaskset() ... (real BE layout)` logs success (r3=0), a
  taskset EA is written.

**Step 2 — cri taskset context + CreateTask(image 22).  [the SPU task appears]**
- Port the fork's real SpursTasksetContext (spurs_taskset.h STC_ offsets:
  TEMP_TASKSET=0x2700, TASK_INFO=0x2780, TASKSET_PTR=0x27B8, SYSCALL=0x27C4=0xA70,
  TASK_ID=0x27D4, SAVED_LR=0x2C80) + spurs_pm_build_context (spurs_pm.c). In the
  image-22 dispatch (spu_workload.c) call spurs_pm_build_context(ls, taskset_ea,...)
  instead of the minimal plant. Credit: JonathanDC64 fork (05e4e3a7, 6b5ea493).
- VERIFY: cri task runs past LS 0x3050 without erroring; log what it reads from
  LS 0x29180 + the inbound mailbox.

**Step 3 — feed the cri task real decode work via its inbound mailbox.  [the crux]**
- cri `func_00003050` reads `SPU_RdInMbox` x3. The game's cri_mpv PPU code sends the
  decode cmd/data-EA. Trace what commands/format the task expects; ensure the
  PPU->SPU mailbox path (sys_spu_thread_write_mbox / HLE dispatch r3/args=eaContext
  @0x0FEFF480) delivers the real video-data EA.
- VERIFY: task consumes the mailbox, DMAs the frame source EA into LS.

**Step 4 — SPU decodes (real recompiled SPU code).  [content produced]**
- Image 22 is already lifted+linked. The policy (lift_pol LS 0xA00) + cri task (LS
  0x3000+) run under the same SPU image. DMA shares vm_base so SPU writes reach PPU.
- VERIFY: SPU writes a decoded frame buffer to an EA in guest RAM.

**Step 5 — completion propagation: SPU done -> q=1.  [wakes the game — HARD, unsolved in fork]**
- On task completion post the REAL SPURS event with the CORRECT payload to the
  attached lv2 queue. The game checks the payload (source-key + data1/data2 =
  which workload/task completed); a generic synth signal FAILS. Wire
  `cellSpursEventFlagSet` (libs/spurs/cellSpurs.c) with the right payload to fire on
  task exit -> attached queue -> q=1. **This step is unsolved even in JonathanDC64's
  fork (commits 2ca55a68/1bd981e1/dd4f603d) — shared frontier, coordinate.**
- VERIFY: thread-1's `event_queue_receive(q=1)` @cia=0x00530E38 RETURNS.

**Step 6 — PPU renders the decoded frame.  [pixels]**
- The woken PPU reads the decoded frame and issues GCM texture/draw methods.
- VERIFY: RSX shows nv4097 draws with the video texture (not just the Scaleform UI);
  D3D12 window shows the intro video, then advances to the menu.

### Constraint compliance (must preserve)
HLE-ing the SPURS *scheduler* is legit; the decode TASK runs as REAL recompiled SPU
code. Only forging the decoded frames or the completion payload would violate the
no-faking rule — don't. (Proven: YDKJ_CRI_WAKE / YDKJ_FORCE_EVF / YDKJ_SPURS_READY
synthetic signals all fail or crash — the game validates real work.)

### Also worth pulling from the fork (game-agnostic, credit JonathanDC64 / caner / sage)
- 7a977e65 / f406890c GCM vblank frame-sema + RSX_TICK_LABELS (frame-loop advance).
- Already adopted: the ppu_lifter correctness fixes (this repo's fix branch).

---
## Original plan (2026-07-03)

State (2026-07-03): the HLE path renders a LIVE D3D12 window (1280x720, clears+flips,
GCM active) but only CLEAR_SURFACE — black, no content. Boot reaches deep init
(SPURS tasks, cellUserInfo, cellSysmodule, cellHttp/cellSsl network) then blocks on
`event_queue_receive(q=1)` at game cia=0x00530E38 + `event_flag_wait(flag=100 bit0x2)`
for real SPURS/cri completions. PROVEN: no synthetic/forced signal advances it
(YDKJ_CRI_WAKE, YDKJ_FORCE_EVF, YDKJ_SPURS_READY all fail / crash) -> content needs
REAL work. Run env for this state: `YDKJ_KEEPGROUP=1 YDKJ_BIGSTACK=1 YDKJ_LV2_SAT=1`
(NO YDKJ_LIBSRE -> HLE cellSpurs). Keeper fix already in tree: cri task bootstrap
(num=0 returns for image 22, spu_channels.c). See memory jonathandc64-fork.md.

## The goal chain (what must actually happen)
game reads intro video -> cri_mpv PPU layer (statically linked in the game, NOT a
PRX) sets up decode + sends work to the cri SPU task (image 22, cri_mpvps3spurs.elf
@0x4F5F80) via its INBOUND MAILBOX -> SPU decodes frames -> posts completion (SPURS
event flag -> attached lv2 queue) -> PPU renders decoded frame via GCM draws.

## Steps (order matters; verify each before the next)

### 1. Real SpursTasksetContext for the cri task  [foundation]
The cri task needs a real context, not my minimal plant. Port the fork's real-SPURS
layer:
- Adopt fork libs/spurs/spurs_taskset.h (STC_ offsets: TEMP_TASKSET=0x2700,
  TEMP_TASKINFO=0x2780, TASKSET_PTR=0x27B8, SYSCALL_ADDR=0x27C4(=0xA70),
  TASK_ID=0x27D4, SAVED_LR=0x2C80) + spurs_pm.c (spurs_pm_build_context).
- My branch's libs/spurs/cellSpurs.c is the SIMPLIFIED DeS-era version (stub
  CellSpursTaskset {u32 initialized; u8 pad[128]}, host s_tasks[] array). Either
  (a) adopt the fork's real-layout cellSpurs.c wholesale (05e4e3a7 P1 +
  6b5ea493 multi-task registry, ~48KB, tuned for DeS -- risky), OR (b) MINIMAL:
  in CreateTask, ALSO write a real CellSpursTaskset+TaskInfo (task_info[0].elf,
  ls_pattern, bitsets) to the guest taskset EA, and in the image-22 dispatch
  (spu_workload.c) call spurs_pm_build_context(ls, taskset_ea, taskId,...) instead
  of my minimal plant. (b) is lower-risk; reuse my CRI_CHAIN taskset layout knowledge
  in ydkj_spurs_kernel.c.
- VERIFY: cri task runs past 0x3050 without erroring; check what it reads from
  LS 0x29180 + the inbound mailbox.

### 2. Feed the cri task real decode work via its inbound mailbox  [the crux]
cri func_00003050 reads SPU_RdInMbox (x3) -- it waits for the PPU cri_mpv layer to
send decode commands/data. The game's cri_mpv PPU code does this, but it CRASHES first
(step 3). Trace: what commands/format the cri task expects in the mailbox (decode a
frame from an EA), and ensure the PPU->SPU mailbox path (sys_spu_thread_write_mbox or
the HLE dispatch's r3/args) delivers it. The dispatch currently passes args=0x0FEFF480
(eaContext). Confirm the real video-data EA reaches the task.

### 3. Fix the cri_mpv PPU-layer crashes (uninitialized objects)  [gate on q=1/flag100]
The game's own lifted cri_mpv code crashes on: (a) null-obj virtual call (obj=0), and
(b) garbage ctr=0xC708C708 (uninitialized fn-ptr; obj=0x400240A8 vtable=0x520BE4 valid
but a slot=garbage). These are the cri_mpv manager not being fully initialized (its
init depends on the SPU tasks being ready). The handler that would set flag=100/post
q=1 crashes here. FIND the crash function: cia=0 in the unresolved-call dump means the
lifter doesn't set ctx->cia at bctrl -- add cia-setting to the ppu_lifter bctrl
emission (or use a read-watch on obj=0x400240A8's vtable slot) to locate the guest
function, then trace why the object/vtable slot is uninitialized (which alloc/init
returned null or was skipped). Likely fixed once steps 1-2 make the SPU side real.

### 4. Real completion propagation (SPU task done -> PPU q=1/flag100)  [wakes the game]
On cri/audio SPU task completion, post the REAL SPURS completion event with the
CORRECT payload to the attached lv2 event queue (the game does
cellSpursEventFlagAttachLv2EventQueue; q=1 is created key=0 size=64). Generic synth
(YDKJ_SPURS_READY) FAILED -> the game checks the payload (which workload/task, the
event source-key). Match the SPURS event format: source-key + data1/data2 identifying
the completed workload. Look at cellSpursEventFlagSet in libs/spurs/cellSpurs.c and
wire it (with the right payload) to fire on task exit -> the attached queue -> q=1.
Cross-ref the fork's completion commits (2ca55a68, 1bd981e1, dd4f603d "SPU->PPU event
chain") -- unsolved there too; SHARED FRONTIER, coordinate with JonathanDC64.

### 5. Verify content
Success at each: DMA from 0x4F5F80 (ELF) present; cri task loops on POLL not EXIT;
q=1/flag100 waits RETURN; RSX shows DRAW/nv4097/texture methods (not just
CLEAR_SURFACE); D3D12 window shows the intro video / menu.

## Also worth pulling from the fork (game-agnostic)
- c3f8590d ppu_lifter jump-table recovery (relift game+libsre after).
- 7a977e65/f406890c GCM vblank frame-sema + RSX_TICK_LABELS (advance frame loop).
- Already adopted: eb5451b3 stfd double-store fix; canersaka SPU+PPU lifter PRs.

## Notes
- HLE-ing cellSpurs (the scheduler) is legit under the no-faking rule; the TASK
  (render-data producer) runs as REAL recompiled SPU code. Only forging the decoded
  frames/completion payloads would violate it -- don't.
- All my session changes are env-gated OFF by default + uncommitted. Bootstrap fix
  is the keeper. This is a multi-session effort; steps 2-4 are the hard core.
