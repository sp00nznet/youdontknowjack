# YDKJ: plan to get visible game content (cri_mpv/SPURS bring-up)

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
