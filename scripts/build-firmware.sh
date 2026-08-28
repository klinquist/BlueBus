#!/bin/sh

set -eu

usage() {
    cat <<'EOF'
Usage: scripts/build-firmware.sh [output.hex]

Build the BlueBus application firmware with the MPLAB X and XC16 versions
configured by firmware/application/nbproject/configurations.xml.

If output.hex is omitted, the named image is placed beside the MPLAB build
artifacts under firmware/application/dist/application/production/.

Environment:
  MPLAB_MAKEGEN  Optional path to prjMakefilesGenerator.sh.
EOF
}

if [ "${1:-}" = "-h" ] || [ "${1:-}" = "--help" ]; then
    usage
    exit 0
fi

if [ "$#" -gt 1 ]; then
    usage >&2
    exit 2
fi

invocation_dir=$(pwd)
script_dir=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
repo_root=$(dirname -- "$script_dir")
project_dir="$repo_root/firmware/application"
project_config="$project_dir/nbproject/configurations.xml"

if [ ! -f "$project_config" ]; then
    echo "error: BlueBus application project was not found at $project_dir" >&2
    exit 1
fi

xml_value() {
    sed -n "s:.*<$1>\([^<]*\)</$1>.*:\1:p" "$project_config" | head -n 1
}

configuration=$(sed -n 's:.*<conf name="\([^"]*\)".*:\1:p' "$project_config" | head -n 1)
xc16_version=$(xml_value languageToolchainVersion)
device_pack_line=$(sed -n '/<pack name=/{p;q;}' "$project_config")
device_pack_name=$(printf '%s\n' "$device_pack_line" | sed -n 's:.*name="\([^"]*\)".*:\1:p')
device_pack_vendor=$(printf '%s\n' "$device_pack_line" | sed -n 's:.*vendor="\([^"]*\)".*:\1:p')
device_pack_version=$(printf '%s\n' "$device_pack_line" | sed -n 's:.*version="\([^"]*\)".*:\1:p')

if [ -z "$configuration" ] || [ -z "$xc16_version" ] ||
   [ -z "$device_pack_name" ] || [ -z "$device_pack_vendor" ] ||
   [ -z "$device_pack_version" ]; then
    echo "error: could not read the MPLAB build configuration" >&2
    exit 1
fi

mplab_makegen=${MPLAB_MAKEGEN:-}
if [ -z "$mplab_makegen" ]; then
    mplab_makegen=$(find /Applications/microchip/mplabx -type f \
        -path '*/mplab_platform/bin/prjMakefilesGenerator.sh' \
        -print 2>/dev/null | sort | tail -n 1)
fi

if [ -z "$mplab_makegen" ] || [ ! -x "$mplab_makegen" ]; then
    echo "error: MPLAB X Makefile generator was not found" >&2
    echo "Install MPLAB X or set MPLAB_MAKEGEN to prjMakefilesGenerator.sh." >&2
    exit 1
fi

xc16_compiler="/Applications/microchip/xc16/v$xc16_version/bin/xc16-gcc"
if [ ! -x "$xc16_compiler" ]; then
    echo "error: XC16 v$xc16_version was not found at $xc16_compiler" >&2
    exit 1
fi

device_pack_dir="${HOME}/.mchp_packs/$device_pack_vendor/$device_pack_name/$device_pack_version"
if [ ! -d "$device_pack_dir" ]; then
    pack_manager=$(dirname -- "$mplab_makegen")/packmanagercli.sh
    if [ ! -x "$pack_manager" ]; then
        echo "error: required device pack is missing and packmanagercli.sh was not found" >&2
        exit 1
    fi
    echo "Installing $device_pack_name $device_pack_version..."
    "$pack_manager" \
        --install-pack "$device_pack_name" \
        --version "$device_pack_version" \
        --vendor "$device_pack_vendor"
fi

if [ ! -d "$device_pack_dir" ]; then
    echo "error: device pack installation did not create $device_pack_dir" >&2
    exit 1
fi

echo "Generating MPLAB Makefiles..."
"$mplab_makegen" "$project_dir@$configuration"

echo "Building BlueBus production firmware with XC16 v$xc16_version..."
make -C "$project_dir" CONF="$configuration" clean build

production_dir="$project_dir/dist/$configuration/production"
production_hex="$production_dir/application.production.hex"
if [ ! -s "$production_hex" ]; then
    echo "error: build completed without producing $production_hex" >&2
    exit 1
fi

firmware_major=$(awk '$2 == "FIRMWARE_VERSION_MAJOR" { print $3; exit }' "$project_dir/mappings.h")
firmware_minor=$(awk '$2 == "FIRMWARE_VERSION_MINOR" { print $3; exit }' "$project_dir/mappings.h")
firmware_patch=$(awk '$2 == "FIRMWARE_VERSION_PATCH" { print $3; exit }' "$project_dir/mappings.h")
commit=$(git -C "$repo_root" rev-parse --short HEAD 2>/dev/null || printf unknown)
if [ -n "$(git -C "$repo_root" status --porcelain --untracked-files=normal 2>/dev/null)" ]; then
    commit="$commit-dirty"
fi

default_name="bluebus_${firmware_major}_${firmware_minor}_${firmware_patch}_${commit}.hex"
if [ "$#" -eq 1 ]; then
    case $1 in
        /*) output_hex=$1 ;;
        *) output_hex="$invocation_dir/$1" ;;
    esac
else
    output_hex="$production_dir/$default_name"
fi

mkdir -p "$(dirname -- "$output_hex")"
cp "$production_hex" "$output_hex"

echo
echo "Firmware build complete:"
echo "  Version: $firmware_major.$firmware_minor.$firmware_patch"
echo "  Commit:  $commit"
echo "  HEX:     $output_hex"
echo "  SHA-256: $(shasum -a 256 "$output_hex" | awk '{print $1}')"
