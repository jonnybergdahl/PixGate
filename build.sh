#!/usr/bin/env bash
#
# PixGate firmware build script.
#
# Compiles every per-device ESPHome configuration in `devices/` and copies the resulting
# firmware images into the `firmware/` folder, one subdirectory per device.
#
# Usage:
#   ./build.sh                       # build every devices/*.yaml
#   ./build.sh wt32-sc01-plus        # build only the matching device(s)
#   ./build.sh devices/foo.yaml      # build a specific file
#
# Requirements:
#   - esphome installed and on PATH (`pip install esphome`).
#
set -euo pipefail

# Resolve the repo root (directory containing this script) so the script works from anywhere.
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
DEVICES_DIR="${ROOT_DIR}/devices"
FIRMWARE_DIR="${ROOT_DIR}/firmware"

if ! command -v esphome >/dev/null 2>&1; then
  echo "error: 'esphome' is not installed or not on PATH. Install it with 'pip install esphome'." >&2
  exit 1
fi

# Build the list of device YAML files to compile.
declare -a YAML_FILES=()
if [[ "$#" -gt 0 ]]; then
  for arg in "$@"; do
    if [[ -f "${arg}" ]]; then
      YAML_FILES+=("${arg}")
    elif [[ -f "${DEVICES_DIR}/${arg}" ]]; then
      YAML_FILES+=("${DEVICES_DIR}/${arg}")
    elif [[ -f "${DEVICES_DIR}/${arg}.yaml" ]]; then
      YAML_FILES+=("${DEVICES_DIR}/${arg}.yaml")
    else
      echo "error: could not find a device YAML matching '${arg}'." >&2
      exit 1
    fi
  done
else
  # No arguments: build every YAML in devices/.
  shopt -s nullglob
  YAML_FILES=("${DEVICES_DIR}"/*.yaml)
  shopt -u nullglob
fi

if [[ "${#YAML_FILES[@]}" -eq 0 ]]; then
  echo "error: no device YAML files found in ${DEVICES_DIR}." >&2
  exit 1
fi

mkdir -p "${FIRMWARE_DIR}"

# Extract the ESPHome node name from a config (used to locate its build directory).
# Prefer the resolved config; fall back to the `name:` substitution in the raw YAML.
device_name() {
  local yaml="$1"
  local name
  name="$(esphome -q config "${yaml}" 2>/dev/null \
    | awk '/^esphome:/{in_es=1; next} in_es && /^[^[:space:]]/{in_es=0} in_es && /^[[:space:]]+name:/{print $2; exit}')"
  if [[ -z "${name}" ]]; then
    name="$(awk '/^substitutions:/{in_sub=1; next} in_sub && /^[^[:space:]]/{in_sub=0} in_sub && /^[[:space:]]+name:/{print $2; exit}' "${yaml}")"
  fi
  echo "${name}"
}

OVERALL_STATUS=0

for yaml in "${YAML_FILES[@]}"; do
  base="$(basename "${yaml}" .yaml)"
  echo ""
  echo "=== Building ${base} (${yaml}) ==="

  if ! esphome compile "${yaml}"; then
    echo "error: compilation failed for ${yaml}" >&2
    OVERALL_STATUS=1
    continue
  fi

  name="$(device_name "${yaml}")"
  if [[ -z "${name}" ]]; then
    echo "warning: could not determine node name for ${yaml}; using '${base}'." >&2
    name="${base}"
  fi

  # ESPHome creates its `.esphome/` working dir next to the config file (here, in
  # devices/), not at the repo root, so resolve the build dir relative to the YAML.
  yaml_dir="$(cd "$(dirname "${yaml}")" && pwd)"
  build_dir="${yaml_dir}/.esphome/build/${name}/.pioenvs/${name}"
  out_dir="${FIRMWARE_DIR}/${base}"
  mkdir -p "${out_dir}"

  copied=0
  for img in firmware.factory.bin firmware.ota.bin firmware.bin; do
    if [[ -f "${build_dir}/${img}" ]]; then
      cp "${build_dir}/${img}" "${out_dir}/${img}"
      copied=1
    fi
  done

  if [[ "${copied}" -eq 0 ]]; then
    echo "warning: no firmware images found in ${build_dir}." >&2
    OVERALL_STATUS=1
  else
    echo "--> firmware copied to ${out_dir}"
  fi
done

echo ""
if [[ "${OVERALL_STATUS}" -eq 0 ]]; then
  echo "All builds completed successfully."
else
  echo "One or more builds failed; see messages above." >&2
fi
exit "${OVERALL_STATUS}"
