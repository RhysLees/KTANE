#!/usr/bin/env bash
# Build one or more KTANE modules by name, or pick interactively from a menu.
# Run from anywhere; paths are resolved relative to this repo root.

set -u

cd "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)" || exit 1

firmwareDir="firmware"
buildDir="build"
EXCLUDE_NAMES=(DOCS shared_libs build firmware .git .vscode .pio)

if [[ -t 1 ]]; then
  CYAN='\033[0;36m'
  GRAY='\033[0;90m'
  GREEN='\033[0;32m'
  YELLOW='\033[0;33m'
  RED='\033[0;31m'
  NC='\033[0m'
else
  CYAN='' GRAY='' GREEN='' YELLOW='' RED='' NC=''
fi

is_excluded() {
  local n=$1 e
  for e in "${EXCLUDE_NAMES[@]}"; do
    [[ "$n" == "$e" ]] && return 0
  done
  return 1
}

file_size_kb() {
  local f=$1 bytes
  if bytes=$(stat -f%z "$f" 2>/dev/null); then
    :
  elif bytes=$(stat -c%s "$f" 2>/dev/null); then
    :
  else
    bytes=$(wc -c <"$f" | tr -d ' \n')
  fi
  awk -v b="$bytes" 'BEGIN { printf "%.2f", b / 1024 }'
}

# Populate global array `modules` (sorted), all dirs with platformio.ini except excluded roots.
discover_modules() {
  modules=()
  local name
  shopt -s nullglob
  for d in */; do
    [[ -d "$d" ]] || continue
    name="${d%/}"
    is_excluded "$name" && continue
    [[ -f "${d}platformio.ini" ]] || continue
    modules+=("$name")
  done
  shopt -u nullglob
  if ((${#modules[@]} > 0)); then
    local _tmp=("${modules[@]}")
    modules=()
    while IFS= read -r line; do
      [[ -n "$line" ]] && modules+=("$line")
    done < <(printf '%s\n' "${_tmp[@]}" | sort -u)
  fi
}

module_is_known() {
  local want=$1 m
  for m in "${modules[@]}"; do
    [[ "$m" == "$want" ]] && return 0
  done
  return 1
}

usage() {
  cat <<'EOF'
Usage: build_module.sh [options] [module ...]

With no arguments, shows an interactive menu to choose a module.

Options:
  -h, --help     Show this help
  -l, --list     List discovered modules and exit

Examples:
  ./build_module.sh              # interactive selection
  ./build_module.sh audio_console
  ./build_module.sh audio timer
EOF
}

build_one() {
  local module=$1
  local build_output build_status found_any uf2 base dest sz

  echo ""
  echo -e "${YELLOW}🔨 Building ${module} module...${NC}"

  if [[ ! -d "$module" ]]; then
    echo -e "${RED}❌ Module directory '${module}' not found${NC}" >&2
    return 1
  fi

  build_output=$(cd "$module" && pio run 2>&1) || true
  build_status=$?

  if [[ $build_status -eq 0 ]]; then
    echo -e "${GREEN}✅ ${module} build successful${NC}"
    found_any=0
    while IFS= read -r uf2; do
      [[ -z "$uf2" ]] && continue
      [[ -f "$uf2" ]] || continue
      found_any=1
      base=$(basename "$uf2")
      dest="${firmwareDir}/${module}_${base}"
      mkdir -p "$firmwareDir"
      cp -f "$uf2" "$dest"
      echo -e "${GREEN}📦 Copied ${base} to ${module}_${base}${NC}"
    done < <(find "${buildDir}/${module}" -type f -name '*.uf2' 2>/dev/null)

    if [[ $found_any -eq 0 ]]; then
      echo -e "${YELLOW}⚠️  No uf2 files found for ${module}${NC}"
    fi
    return 0
  fi

  echo -e "${RED}❌ ${module} build failed${NC}" >&2
  echo -e "${RED}${build_output}${NC}" >&2
  return 1
}

# --- parse args ---
to_build=()
while (($# > 0)); do
  case "$1" in
    -h | --help)
      usage
      exit 0
      ;;
    -l | --list)
      discover_modules
      if ((${#modules[@]} == 0)); then
        echo -e "${RED}❌ No modules found (need platformio.ini in a subfolder).${NC}" >&2
        exit 1
      fi
      echo -e "${CYAN}Modules:${NC}"
      printf '%s\n' "${modules[@]}"
      exit 0
      ;;
    --)
      shift
      to_build+=("$@")
      break
      ;;
    -*)
      echo -e "${RED}Unknown option: $1${NC}" >&2
      usage >&2
      exit 1
      ;;
    *)
      to_build+=("$1")
      ;;
  esac
  shift
done

discover_modules
if ((${#modules[@]} == 0)); then
  echo -e "${RED}❌ No modules found! Add directories with platformio.ini under the repo root.${NC}" >&2
  exit 1
fi

if ((${#to_build[@]} == 0)); then
  if [[ ! -t 0 ]]; then
    echo -e "${RED}❌ No module given and stdin is not a TTY. Pass a name or use -l.${NC}" >&2
    usage >&2
    exit 1
  fi
  echo -e "${CYAN}🔧 KTANE — select module to build${NC}"
  echo -e "${CYAN}===================================${NC}"
  echo ""
  PS3="$(echo -e "${YELLOW}Enter number: ${NC}")"
  select choice in "${modules[@]}" "Cancel"; do
    if [[ "$choice" == "Cancel" ]]; then
      echo "Cancelled."
      exit 0
    fi
    if [[ -n "${choice:-}" ]]; then
      to_build=("$choice")
      break
    fi
    echo -e "${RED}Invalid selection.${NC}" >&2
  done
  echo ""
fi

# Validate names
bad=()
for name in "${to_build[@]}"; do
  module_is_known "$name" || bad+=("$name")
done
if ((${#bad[@]} > 0)); then
  echo -e "${RED}❌ Unknown module(s): ${bad[*]}${NC}" >&2
  echo -e "${GRAY}Use: ./build_module.sh -l${NC}" >&2
  exit 1
fi

echo -e "${CYAN}🔧 KTANE single-module build${NC}"
echo -e "${CYAN}============================${NC}"
echo -e "${GREEN}📋 Building: ${to_build[*]}${NC}"

successCount=0
failCount=0
for module in "${to_build[@]}"; do
  if build_one "$module"; then
    ((successCount++)) || true
  else
    ((failCount++)) || true
  fi
done

echo ""
echo -e "${CYAN}📊 Build Summary:${NC}"
echo -e "${CYAN}=================${NC}"
echo -e "${GREEN}✅ Successful: ${successCount}${NC}"
echo -e "${RED}❌ Failed: ${failCount}${NC}"

if ((successCount > 0)); then
  echo ""
  echo -e "${GREEN}📁 Copied UF2(s) under '${firmwareDir}':${NC}"
  for module in "${to_build[@]}"; do
    shopt -s nullglob
    for f in "${firmwareDir}/${module}"_*.uf2; do
      [[ -f "$f" ]] || continue
      sz=$(file_size_kb "$f")
      echo -e "${GRAY}  • $(basename "$f") (${sz} KB)${NC}"
    done
    shopt -u nullglob
  done
fi

echo ""
if ((failCount == 0)); then
  echo -e "${GREEN}🎉 Done.${NC}"
else
  echo -e "${YELLOW}⚠️  One or more builds failed.${NC}"
  exit 1
fi
