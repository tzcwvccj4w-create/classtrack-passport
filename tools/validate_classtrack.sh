#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "$0")/.." && pwd)"
build_dir="${TMPDIR:-/tmp}/classtrack-host-tests"
read -r -a host_cc <<< "${CC:-cc}"
mkdir -p "$build_dir"

"${host_cc[@]}" -std=c11 -Wall -Wextra -Werror -DCT_HOST_TEST -I"$repo_root/main" \
  "$repo_root/tests/test_classtrack_core.c" \
  "$repo_root/main/ct_adpcm.c" "$repo_root/main/ct_crc32.c" \
  "$repo_root/main/ct_store.c" "$repo_root/main/ct_classes.c" \
  "$repo_root/main/ct_protocol.c" -o "$build_dir/test_classtrack_core"
"$build_dir/test_classtrack_core"
python3 "$repo_root/tests/test_manager.py"
python3 "$repo_root/tests/test_mac_app.py"
python3 "$repo_root/tests/test_classtrack_runtime_contract.py"

echo "PASS: ClassTrack host gate"
