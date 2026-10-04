#!/usr/bin/env bash
set -e
set -o pipefail

BUILD_DIR="build"
DIST_DIR="dist"
BUILD_TYPE="Release"
RAYLIB_PATH=""
DOUKUTSU_PATH=""
PACKAGE=0
PACKAGE_VER=""
CLEAN=0
CMAKE_FLAGS=()

print_help() {
    cat << EOF
Usage: ./build.sh [OPTIONS] [-- <additional cmake flags>]

Options:
  --raylib-path <path>    Specify local Raylib directory (lib/ and include/)
  --doukutsu <path>       Path to Doukutsu directory or executable to extract assets
  --csplus <path>         Path to Cave Story+ assets directory (containing base/)
  -p, --package [ver]     Build and create .tar.gz distribution package
  -c, --clean             Remove build directory before configuring
  -d, --debug             Configure build in Debug mode
  -r, --release           Configure build in Release mode (default)
  -h, --help              Show this help message

Examples:
  ./build.sh --raylib-path /usr/local
  ./build.sh --doukutsu /path/to/Doukutsu
  ./build.sh -c -r
  ./build.sh -p v2.6.5
EOF
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --raylib-path)
            RAYLIB_PATH="$2"
            shift 2
            ;;
        --doukutsu|--doukutsu-path)
            DOUKUTSU_PATH="$2"
            shift 2
            ;;
        --csplus|--csplus-path)
            CSPLUS_PATH="$2"
            shift 2
            ;;
        -p|--package)
            PACKAGE=1
            BUILD_TYPE="Release"
            if [[ $# -gt 1 && "$2" != -* ]]; then
                PACKAGE_VER="$2"
                shift 2
            else
                shift
            fi
            ;;
        -c|--clean)
            CLEAN=1
            shift
            ;;
        -d|--debug)
            BUILD_TYPE="Debug"
            shift
            ;;
        -r|--release)
            BUILD_TYPE="Release"
            shift
            ;;
        -h|--help)
            print_help
            exit 0
            ;;
        --)
            shift
            CMAKE_FLAGS+=("$@")
            break
            ;;
        *)
            CMAKE_FLAGS+=("$1")
            shift
            ;;
    esac
done

if [ "$CLEAN" -eq 1 ] && [ -d "$BUILD_DIR" ]; then
    echo "==> Cleaning build directory..."
    rm -rf "$BUILD_DIR"
fi

CMAKE_CONFIG_ARGS=(
    -B "$BUILD_DIR"
    -DCMAKE_BUILD_TYPE="$BUILD_TYPE"
)

if [ -n "$RAYLIB_PATH" ]; then
    CMAKE_CONFIG_ARGS+=("-DRAYLIB_PATH=$RAYLIB_PATH")
fi

if [ ${#CMAKE_FLAGS[@]} -gt 0 ]; then
    CMAKE_CONFIG_ARGS+=("${CMAKE_FLAGS[@]}")
fi

echo "==> Configuring with: cmake ${CMAKE_CONFIG_ARGS[*]}"
cmake "${CMAKE_CONFIG_ARGS[@]}"

# Extract Doukutsu assets if path specified
if [ -n "$DOUKUTSU_PATH" ]; then
    echo "==> Building extractor tool..."
    cmake --build "$BUILD_DIR" --target nxextract --parallel

    DEST_DOUKUTSU="data/doukutsu_data"
    EXE_FILE=""
    SRC_DATA_DIR=""

    if [ -f "$DOUKUTSU_PATH" ]; then
        EXE_FILE="$DOUKUTSU_PATH"
        PARENT_DIR="$(dirname "$DOUKUTSU_PATH")"
        [ -d "${PARENT_DIR}/data" ] && SRC_DATA_DIR="${PARENT_DIR}/data"
    elif [ -d "$DOUKUTSU_PATH" ]; then
        for candidate in "$DOUKUTSU_PATH/Doukutsu.exe" "$DOUKUTSU_PATH/doukutsu.exe" "$DOUKUTSU_PATH/DOUKUTSU.EXE"; do
            if [ -f "$candidate" ]; then
                EXE_FILE="$candidate"
                break
            fi
        done
        [ -d "$DOUKUTSU_PATH/data" ] && SRC_DATA_DIR="$DOUKUTSU_PATH/data"
    fi

    if [ -z "$EXE_FILE" ] || [ ! -f "$EXE_FILE" ]; then
        echo "Error: Doukutsu.exe not found at '$DOUKUTSU_PATH'" >&2
        exit 1
    fi

    mkdir -p "$DEST_DOUKUTSU"

    # Copy files data folder
    if [ -n "$SRC_DATA_DIR" ] && [ -d "$SRC_DATA_DIR" ]; then
        echo "==> Copying Doukutsu data files..."
        cp -r "$SRC_DATA_DIR"/* "$DEST_DOUKUTSU"/
    fi

    # Extract assets from Doukutsu.exe
    echo "==> Extracting executable assets from $EXE_FILE..."
    "${BUILD_DIR}/nxextract" "$EXE_FILE" "$DEST_DOUKUTSU"
fi

# Copy CSPlus assets and soundtrack folders if path specified
if [ -n "$CSPLUS_PATH" ]; then
    echo "==> Setting up Cave Story+ assets..."
    DEST_CSPLUS="data/csplus_data/base"
    DEST_OSTS="data/osts"
    mkdir -p "$DEST_CSPLUS"
    mkdir -p "$DEST_OSTS"

    SRC_BASE=""
    if [ -d "$CSPLUS_PATH/base" ]; then
        SRC_BASE="$CSPLUS_PATH/base"
    elif [ -d "$CSPLUS_PATH/Base" ]; then
        SRC_BASE="$CSPLUS_PATH/Base"
    elif [ -d "$CSPLUS_PATH" ]; then
        SRC_BASE="$CSPLUS_PATH"
    fi

    if [ -n "$SRC_BASE" ] && [ -d "$SRC_BASE" ]; then
        echo "==> Storing CS+ data in $DEST_CSPLUS..."
        cp -r "$SRC_BASE"/* "$DEST_CSPLUS"/

        echo "==> Moving soundtrack tracks to $DEST_OSTS..."
        for d in ogg ogg11 ogg17 ogg_ridic org pixtone Ogg Ogg11 Ogg17 Org PixTone; do
            if [ -d "$SRC_BASE/$d" ]; then
                target_d="$(echo "$d" | tr '[:upper:]' '[:lower:]')"
                mkdir -p "$DEST_OSTS/$target_d"
                cp -r "$SRC_BASE/$d"/* "$DEST_OSTS/$target_d/"
            fi
        done
    else
        echo "Warning: Could not find base directory in '$CSPLUS_PATH'" >&2
    fi
fi

# Move already present data if csplus_data exists but data/osts does not
if [ -d "data/csplus_data/base" ] && [ ! -d "data/osts" ]; then
    echo "==> Changing existing soundtracks to data/osts..."
    mkdir -p "data/osts"
    for d in ogg ogg11 ogg17 ogg_ridic org pixtone Ogg Ogg11 Ogg17 Org PixTone; do
        if [ -d "data/csplus_data/base/$d" ]; then
            target_d="$(echo "$d" | tr '[:upper:]' '[:lower:]')"
            mkdir -p "data/osts/$target_d"
            cp -r "data/csplus_data/base/$d"/* "data/osts/$target_d/"
        fi
    done
fi

echo "==> Building project..."
BUILD_CMD=(cmake --build "$BUILD_DIR" --parallel)
if command -v stdbuf >/dev/null 2>&1; then
    BUILD_CMD=(stdbuf -oL -eL "${BUILD_CMD[@]}")
fi

if [ -t 1 ]; then
    "${BUILD_CMD[@]}" 2>&1 | while IFS= read -r line; do
        if [[ "$line" =~ ^\[[[:space:]]*([0-9]+(%|/[0-9]+))\][[:space:]]*(.*) ]]; then
            pct="${BASH_REMATCH[1]}"
            rest="${BASH_REMATCH[3]}"

            if [[ "$rest" =~ Building[[:space:]]C(XX)?[[:space:]]object[[:space:]]+(.*) ]]; then
                file="${BASH_REMATCH[2]}"
                file="${file#CMakeFiles/*.dir/}"
                file="${file%.o}"
                printf "\r\033[K\033[1;32m[%4s]\033[0m %s" "$pct" "$file"
            elif [[ "$rest" =~ Linking[[:space:]]+(.*) ]]; then
                target="${BASH_REMATCH[1]}"
                target="${target#*executable }"
                target="${target#*library }"
                printf "\r\033[K\033[1;36m[%4s]\033[0m Linking %s" "$pct" "$target"
            elif [[ "$rest" =~ Built[[:space:]]target[[:space:]]+(.*) ]]; then
                printf "\r\033[K\033[1;34m[%4s]\033[0m Built %s" "$pct" "${BASH_REMATCH[1]}"
            fi
        else
            printf "\n%s" "$line"
        fi
    done
    printf "\r\033[K\033[1;32m==> Build complete.\033[0m\n"
else
    "${BUILD_CMD[@]}"
fi

# Package distribution tarball
if [ "$PACKAGE" -eq 1 ]; then
    echo "==> Packaging release..."

    VERSION="$PACKAGE_VER"
    if [ -z "$VERSION" ]; then
        if command -v git >/dev/null 2>&1 && git rev-parse --is-inside-work-tree >/dev/null 2>&1; then
            VERSION=$(git describe --tags --always 2>/dev/null || echo "v2.6.5")
        else
            VERSION="v2.6.5"
        fi
    fi
    [[ "$VERSION" != v* ]] && VERSION="v${VERSION}"

    OS_NAME="$(uname -s | tr '[:upper:]' '[:lower:]')"
    RAW_ARCH="$(uname -m)"
    case "$RAW_ARCH" in
        x86_64|amd64) ARCH="x86_64" ;;
        aarch64|arm64) ARCH="aarch64" ;;
        *) ARCH="$RAW_ARCH" ;;
    esac

    calc_sha256() {
        local file="$1"
        if command -v sha256sum >/dev/null 2>&1; then
            sha256sum "$file"
        elif command -v shasum >/dev/null 2>&1; then
            shasum -a 256 "$file"
        else
            openssl dgst -sha256 "$file" | awk '{print $NF "  " "'"$file"'"}'
        fi
    }

    mkdir -p "$DIST_DIR"
    PKG_NAME="nxengine-ne-${VERSION}-${OS_NAME}-${ARCH}"
    PKG_STAGE="${DIST_DIR}/${PKG_NAME}"
    rm -rf "$PKG_STAGE"
    mkdir -p "$PKG_STAGE"

    cp "${BUILD_DIR}/nxengine-ne" "${PKG_STAGE}/"
    [ -f "${BUILD_DIR}/nxextract" ] && cp "${BUILD_DIR}/nxextract" "${PKG_STAGE}/"
    [ -f "${BUILD_DIR}/nxtranslate" ] && cp "${BUILD_DIR}/nxtranslate" "${PKG_STAGE}/"
    command -v strip >/dev/null 2>&1 && strip -s "${PKG_STAGE}/nxengine-ne" 2>/dev/null || true

    [ -d "data" ] && cp -r data "${PKG_STAGE}/"
    [ -d "resources" ] && cp -r resources "${PKG_STAGE}/"

    for doc in README.md LICENSE; do
        [ -f "$doc" ] && cp "$doc" "${PKG_STAGE}/"
    done

    PKG_TAR="${PKG_NAME}.tar.gz"
    tar -czf "${DIST_DIR}/${PKG_TAR}" -C "$DIST_DIR" "$PKG_NAME"
    (cd "$DIST_DIR" && calc_sha256 "$PKG_TAR" > "${PKG_TAR}.sha256")
    rm -rf "$PKG_STAGE"

    echo -e "\033[1;32m==> Created ${DIST_DIR}/${PKG_TAR}\033[0m"
fi