SPURS SPU kernel binaries extracted from libsre.prx (Sony cellSpurs/cellSync).

Two embedded SPU ELF images (e_machine=23 SPU, ET_EXEC):
  spurs_kernel_a.elf  <- libsre.prx file offset 0x020480, entry LS 0x818,
                         PT_LOAD vaddr 0x100 filesz 0x780 (1920 bytes / 480 insns).
  spurs_kernel_b.elf  <- libsre.prx file offset 0x020D00, entry LS 0x848,
                         PT_LOAD vaddr 0x100 filesz 0x790.
Confirmed real SPU code (spu_lifter --auto-functions: 11 funcs, 76.5% coverage).

These are the SPURS kernel/policy-module the 5 cellSpurs SPU threads must run.
Lift: python ps3/tools/spu_lifter.py --auto-functions spurs_kernel_a.elf \
        --symbol-prefix sk_a_ -o lift_a   (and likewise _b).

WHY THEY MATTER: with libsre integrated, cellSpursInitialize creates the 5 SPU
threads but hands them an EMPTY sys_spu_image (entry=0; it never loads its kernel
in our env) and never calls sys_spu_thread_group_start. The SPU side is therefore
inert and the PPU busy-waits on an SPU completion that never comes -> 0 GCM draws.

REMAINING WIRING (the path to real SPU execution -> render):
  1. Make cellSpurs load the kernel into the SPU image (entry/segs) -- it currently
     does not (investigate the unresolved sysPrxForUser import 0xEBE5F72F libsre
     calls during init, and/or implement sys_spu_image_import to parse the kernel).
  2. Register the lifted kernel + make sys_spu_thread_group_start run it.
  3. Kernel claims workloads from libsre's (now correctly-written) taskset and
     dispatches the title's lifted SPU task images (image 22 = spu_0021_at_004E5F80).
  4. Bridge SPU completion (WrOutIntrMbox / DMA completion counter) -> the PPU's
     event queue / completion poll so init unblocks -> asset load -> GCM draws.
