#!/bin/sh

set -eu

usage() {
    cat <<'EOF'
Usage: scripts/build-firmware.sh [options] [output.hex]

Build the BlueBus application firmware with the MPLAB X and XC16 versions
configured by firmware/application/nbproject/configurations.xml.

Source options:
  --source current     Build the current working tree, including local changes.
  --source upstream    Build origin/master, or origin/NAME with --branch NAME.
  --source fork        Build fork/master, or fork/NAME with --branch NAME.
  --source local       Build the local branch selected by --branch NAME.
  --branch NAME        Select a branch within upstream, fork, or local.

If --source is omitted in an interactive terminal, the script prompts for a
source. Non-interactive callers must specify --source.

If output.hex is omitted, the named image is placed under the current
repository's firmware/application/dist/application/production/ directory.

Environment:
  MPLAB_MAKEGEN  Optional path to prjMakefilesGenerator.sh.
EOF
}

die() {
    echo "error: $*" >&2
    exit 1
}

invocation_dir=$(pwd)
script_dir=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
repo_root=$(dirname -- "$script_dir")

source_choice=
branch_name=
output_arg=

while [ "$#" -gt 0 ]; do
    case $1 in
        -h|--help)
            usage
            exit 0
            ;;
        --source)
            [ "$#" -ge 2 ] || die "--source requires a value"
            source_choice=$2
            shift 2
            ;;
        --source=*)
            source_choice=${1#*=}
            shift
            ;;
        --branch)
            [ "$#" -ge 2 ] || die "--branch requires a value"
            branch_name=$2
            shift 2
            ;;
        --branch=*)
            branch_name=${1#*=}
            shift
            ;;
        --)
            shift
            if [ "$#" -gt 1 ] || { [ "$#" -eq 1 ] && [ -n "$output_arg" ]; }; then
                usage >&2
                exit 2
            fi
            if [ "$#" -eq 1 ]; then
                output_arg=$1
            fi
            break
            ;;
        -*)
            echo "error: unknown option: $1" >&2
            usage >&2
            exit 2
            ;;
        *)
            if [ -n "$output_arg" ]; then
                usage >&2
                exit 2
            fi
            output_arg=$1
            shift
            ;;
    esac
done

prompt_source() {
    if [ ! -t 0 ]; then
        die "--source is required when standard input is not interactive"
    fi

    echo "Choose firmware source:" >&2
    echo "  1) Current working tree" >&2
    echo "  2) Upstream master (origin/master)" >&2
    echo "  3) Fork master (fork/master)" >&2
    echo "  4) Another branch" >&2
    printf "Selection [1-4]: " >&2
    IFS= read -r selection

    case $selection in
        1) source_choice=current ;;
        2) source_choice=upstream ;;
        3) source_choice=fork ;;
        4)
            echo "Choose branch location:" >&2
            echo "  1) Upstream (origin)" >&2
            echo "  2) Fork" >&2
            echo "  3) Local branch" >&2
            printf "Selection [1-3]: " >&2
            IFS= read -r location
            case $location in
                1) source_choice=upstream ;;
                2) source_choice=fork ;;
                3) source_choice=local ;;
                *) die "invalid branch location selection" ;;
            esac
            printf "Branch name: " >&2
            IFS= read -r branch_name
            ;;
        *) die "invalid source selection" ;;
    esac
}

if [ -z "$source_choice" ]; then
    prompt_source
fi

case $source_choice in
    current)
        if [ -n "$branch_name" ]; then
            die "--branch cannot be used with --source current"
        fi
        ;;
    upstream|fork)
        if [ -z "$branch_name" ]; then
            branch_name=master
        fi
        ;;
    local)
        if [ -z "$branch_name" ]; then
            die "--source local requires --branch NAME"
        fi
        ;;
    *)
        die "--source must be current, upstream, fork, or local"
        ;;
esac

if [ -n "$branch_name" ] &&
   ! git check-ref-format --branch "$branch_name" >/dev/null 2>&1; then
    die "invalid branch name: $branch_name"
fi

git -C "$repo_root" rev-parse --git-dir >/dev/null 2>&1 ||
    die "$repo_root is not a Git repository"

build_root=$repo_root
source_label=current
source_commit=$(git -C "$repo_root" rev-parse --verify 'HEAD^{commit}')
commit=$(git -C "$repo_root" rev-parse --short "$source_commit")
temporary_dir=
worktree_added=0

cleanup() {
    if [ "$worktree_added" -eq 1 ]; then
        git -C "$repo_root" worktree remove --force "$build_root" >/dev/null 2>&1 || true
        worktree_added=0
    fi
    if [ -n "$temporary_dir" ] && [ -d "$temporary_dir" ]; then
        rmdir "$temporary_dir" >/dev/null 2>&1 || true
    fi
}

handle_signal() {
    signal_status=$1
    trap - EXIT HUP INT TERM
    cleanup
    exit "$signal_status"
}

trap cleanup EXIT
trap 'handle_signal 129' HUP
trap 'handle_signal 130' INT
trap 'handle_signal 143' TERM

if [ "$source_choice" = current ]; then
    if [ -n "$(git -C "$repo_root" status --porcelain --untracked-files=normal)" ]; then
        commit="$commit-dirty"
    fi
else
    if [ "$source_choice" = local ]; then
        source_ref="refs/heads/$branch_name"
        source_label="local-$branch_name"
    else
        if [ "$source_choice" = upstream ]; then
            remote_name=origin
        else
            remote_name=fork
        fi
        git -C "$repo_root" remote get-url "$remote_name" >/dev/null 2>&1 ||
            die "Git remote '$remote_name' is not configured"
        echo "Fetching $remote_name..."
        git -C "$repo_root" fetch "$remote_name"
        source_ref="refs/remotes/$remote_name/$branch_name"
        source_label="$source_choice-$branch_name"
    fi

    source_commit=$(git -C "$repo_root" rev-parse --verify "$source_ref^{commit}" 2>/dev/null) ||
        die "branch '$branch_name' was not found for source '$source_choice'"
    commit=$(git -C "$repo_root" rev-parse --short "$source_commit")
    temporary_dir=$(mktemp -d "${TMPDIR:-/tmp}/bluebus-firmware.XXXXXX")
    build_root="$temporary_dir/worktree"
    echo "Preparing $source_label at $commit..."
    git -C "$repo_root" worktree add --detach "$build_root" "$source_commit" >/dev/null
    worktree_added=1
fi

project_dir="$build_root/firmware/application"
project_config="$project_dir/nbproject/configurations.xml"

if [ ! -f "$project_config" ]; then
    die "BlueBus application project was not found at $project_dir"
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
    die "could not read the MPLAB build configuration"
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
    die "XC16 v$xc16_version was not found at $xc16_compiler"
fi

device_pack_dir="${HOME}/.mchp_packs/$device_pack_vendor/$device_pack_name/$device_pack_version"
if [ ! -d "$device_pack_dir" ]; then
    pack_manager=$(dirname -- "$mplab_makegen")/packmanagercli.sh
    if [ ! -x "$pack_manager" ]; then
        die "required device pack is missing and packmanagercli.sh was not found"
    fi
    echo "Installing $device_pack_name $device_pack_version..."
    "$pack_manager" \
        --install-pack "$device_pack_name" \
        --version "$device_pack_version" \
        --vendor "$device_pack_vendor"
fi

if [ ! -d "$device_pack_dir" ]; then
    die "device pack installation did not create $device_pack_dir"
fi

echo "Generating MPLAB Makefiles..."
"$mplab_makegen" "$project_dir@$configuration"

echo "Building BlueBus production firmware with XC16 v$xc16_version..."
make -C "$project_dir" CONF="$configuration" clean build

production_dir="$project_dir/dist/$configuration/production"
production_hex="$production_dir/application.production.hex"
if [ ! -s "$production_hex" ]; then
    die "build completed without producing $production_hex"
fi

firmware_major=$(awk '$2 == "FIRMWARE_VERSION_MAJOR" { print $3; exit }' "$project_dir/mappings.h")
firmware_minor=$(awk '$2 == "FIRMWARE_VERSION_MINOR" { print $3; exit }' "$project_dir/mappings.h")
firmware_patch=$(awk '$2 == "FIRMWARE_VERSION_PATCH" { print $3; exit }' "$project_dir/mappings.h")

safe_source_label=$(printf '%s' "$source_label" | tr '/ ' '--' | tr -cd 'A-Za-z0-9._-')
if [ "$source_choice" = current ]; then
    default_name="bluebus_${firmware_major}_${firmware_minor}_${firmware_patch}_${commit}.hex"
else
    default_name="bluebus_${firmware_major}_${firmware_minor}_${firmware_patch}_${safe_source_label}_${commit}.hex"
fi

if [ -n "$output_arg" ]; then
    case $output_arg in
        /*) output_hex=$output_arg ;;
        *) output_hex="$invocation_dir/$output_arg" ;;
    esac
else
    output_hex="$repo_root/firmware/application/dist/application/production/$default_name"
fi

mkdir -p "$(dirname -- "$output_hex")"
cp "$production_hex" "$output_hex"

echo
echo "Firmware build complete:"
echo "  Version: $firmware_major.$firmware_minor.$firmware_patch"
echo "  Source:  $source_label"
echo "  Commit:  $commit"
echo "  HEX:     $output_hex"
echo "  SHA-256: $(shasum -a 256 "$output_hex" | awk '{print $1}')"
