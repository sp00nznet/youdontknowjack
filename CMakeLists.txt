# You Don't Know Jack (BLUS30569) — minimal PPU boot harness.
#
# Wires the lifter output to the generic first-boot harness shipped with
# ps3recomp (runtime/ppu/tests/boot_main.cpp): load EBOOT -> register lifted
# functions + HLE NID handlers -> dispatch the entry OPD -> run real boot code
# until it hits an unlifted function / unimplemented import / lv2 syscall, each
# logged so we know exactly what to implement next.
#
# Toolchain: clang-cl (the generic loader uses __builtin_bswap / weak symbols
# that MSVC lacks; clang-cl keeps the MSVC ABI so it links the prebuilt
# ps3recomp_runtime.lib). Configure from a VS dev shell with LLVM on PATH:
#
#   cmake -S . -B build -G Ninja -DCMAKE_C_COMPILER=clang-cl -DCMAKE_CXX_COMPILER=clang-cl
#   cmake --build build
#   ./build/ydkj_boot "input/EBOOT.elf"
cmake_minimum_required(VERSION 3.20)
project(ydkj_boot CXX C)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_C_STANDARD 17)

set(PS3RECOMP_DIR "D:/recomp/ps3" CACHE PATH "ps3recomp repo root")
set(PS3RECOMP_RUNTIME_LIB "${PS3RECOMP_DIR}/build/Release/ps3recomp_runtime.lib"
    CACHE FILEPATH "prebuilt runtime library")

# Lifted game code (chunked .cpp) + the generated HLE NID table.
file(GLOB RECOMP_SOURCES "src/recomp/*.c" "src/recomp/*.cpp")
file(GLOB GEN_SOURCES    "src/gen/*.cpp")
# Lifted SPU images (one spu_recomp.c per embedded SPU ELF) + workload registry.
file(GLOB SPU_SOURCES    "src/spu_gen/*/spu_recomp.c" "src/spu_gen/ydkj_spu_workloads.c")

# Real system-PRX modules (libsre = Sony cellSpurs/cellSync). Lifted + namespaced
# (libsre_* prefix) so it links alongside the title; the runtime PRX loader
# places the relocated image (prx/libsre.linked.bin) into guest RAM and registers
# its exports so cellSpurs imports dispatch into real recompiled Sony code instead
# of HLE stubs. Reused from the flОw project (firmware-version-close; covers 66/82
# of YDKJ's SPURS NIDs). The lifted .c uses C++ constructs -> force C++.
file(GLOB PRX_MODULE_SOURCES
    "prx/libsre_ns/ppu_recomp.c"
    "prx/libsre_ns/libsre_exports.c"
    "prx/libsre_ns/libsre_externs.cpp"
)
if(PRX_MODULE_SOURCES)
    set_source_files_properties(${PRX_MODULE_SOURCES} PROPERTIES LANGUAGE CXX)
endif()

add_executable(ydkj_boot
    "${PS3RECOMP_DIR}/runtime/ppu/tests/boot_main.cpp"   # generic boot harness
    "${PS3RECOMP_DIR}/runtime/ppu/ppu_loader.cpp"        # vm + ELF loader + run
    "${PS3RECOMP_DIR}/runtime/ppu/ppu_hle.cpp"           # NID -> handler dispatch
    "${PS3RECOMP_DIR}/runtime/ppu/ppu_sysprx.cpp"        # boot-critical CRT
    "${PS3RECOMP_DIR}/runtime/ppu/ppu_fs.cpp"            # cellFs VFS
    "${PS3RECOMP_DIR}/runtime/prx/prx_loader.c"          # real-PRX loader (libsre)
    "${CMAKE_CURRENT_SOURCE_DIR}/src/prx_init.cpp"       # libsre load hook
    ${GEN_SOURCES}
    ${RECOMP_SOURCES}
    ${SPU_SOURCES}
    ${PRX_MODULE_SOURCES}
)

# Every libs/* subdir (the generated NID table #includes each module's header).
file(GLOB LIB_INC_DIRS LIST_DIRECTORIES true "${PS3RECOMP_DIR}/libs/*")

target_include_directories(ydkj_boot PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src/compat"            # <dirent.h>/<unistd.h> Win shims
    "${CMAKE_CURRENT_SOURCE_DIR}/src/recomp"            # ppu_recomp.h
    "${PS3RECOMP_DIR}/runtime/ppu"
    "${PS3RECOMP_DIR}/runtime/spu"
    "${PS3RECOMP_DIR}/runtime/prx"
    "${PS3RECOMP_DIR}/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/prx/libsre_ns"          # libsre ppu_recomp.h
    ${LIB_INC_DIRS}
)

target_link_libraries(ydkj_boot PRIVATE "${PS3RECOMP_RUNTIME_LIB}")
if(WIN32)
    target_link_libraries(ydkj_boot PRIVATE ws2_32 bcrypt d3d12 dxgi dxguid d3dcompiler)
    # Deep recompiled call chains nest on the host stack (each guest call is a
    # host call); reserve 256 MB for the main thread so it doesn't overflow the
    # 1 MB default. Spawned threads get their reserve in sys_ppu_thread_create.
    target_link_options(ydkj_boot PRIVATE /STACK:268435456 /MAP:ydkj.map)
endif()
