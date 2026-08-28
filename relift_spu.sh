#!/usr/bin/env bash
# Regenerate src/spu_gen/<image>/spu_recomp.{c,h} from the extracted SPU ELFs.
#
# src/spu_gen/ is gitignored, so without this script the lifted tree exists only
# on whichever machine last produced it -- which is exactly how flOw's tree
# became unreproducible. Run this instead of hand-editing generated code.
#
#   ./relift_spu.sh            # all images
#   ./relift_spu.sh spu_0000_at_0049EC00 spu_0002_at_004BCA80
#
# The symbol prefix MUST be "<image-dir-name>_": every image defines its own
# spu_func_* / spu_recomp_register, and they all link into one binary.
# src/spu_gen/ydkj_spu_workloads.c registers images by content fingerprint of
# the ELF, so it only needs regenerating if spu/*.elf themselves change.
#
# NOTE: image 22 (the SPURS kernel/policy) is driven by src/ydkj_spurs_kernel.c
# and has hand-tuned history -- re-lift it only deliberately.
set -euo pipefail

LIFTER="${LIFTER:-G:/recomp/ps3games/ps3recomp/tools/spu_lifter.py}"
here="$(cd "$(dirname "$0")" && pwd)"
cd "$here"

imgs=("$@")
if [ ${#imgs[@]} -eq 0 ]; then
  imgs=()
  for e in spu/spu_*.elf; do imgs+=("$(basename "$e" .elf)"); done
fi

for img in "${imgs[@]}"; do
  elf="spu/$img.elf"
  out="src/spu_gen/$img"
  [ -f "$elf" ] || { echo "!! no such ELF: $elf" >&2; exit 1; }
  mkdir -p "$out"
  echo "--- $img"
  python "$LIFTER" --auto-functions "$elf" --output "$out" \
      --symbol-prefix "${img}_" --source-name spu_recomp.c --header-name spu_recomp.h \
    | grep -E "function\(s\) lifted|unsupported instruction|Function coverage" || true
done
echo "done -- rebuild with: cmake --build build-g"
