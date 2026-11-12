#!/usr/bin/env bash
# KTANE Module Build Script
# Builds all module firmwares and copies uf2 files to firmware directory
# macOS/Linux equivalent of build_all.ps1

set -u

firmwareDir="firmware"
buildDir="build"

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

relative_up() {
  local path=$1 depth
  path="${path//\\//}"
  depth=$(awk -F'/' '{print NF}' <<<"$path")
  local i
  for ((i = 0; i < depth; i++)); do
    printf '../'
  done
}

leaf_name() {
  local path=$1
  path="${path//\\//}"
  basename "$path"
}

discover_firmware_modules() {
  modules=()
  local root child rel exclude
  for root in modules edgework support; do
    [[ -d "$root" ]] || continue
  for child in "$root"/*/; do
      [[ -d "$child" ]] || continue
      exclude=0
      if [[ "$root" == support && "$(basename "$child")" == debugger ]]; then
        exclude=1
      fi
      ((exclude)) && continue
      [[ -f "${child}platformio.ini" ]] || continue
      rel="${child%/}"
      modules+=("$rel")
      echo -e "${GRAY}  • Found module: ${rel}${NC}"
    done
  done
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

echo -e "${CYAN}🔧 KTANE Module Build Script${NC}"
echo -e "${CYAN}=============================${NC}"

echo -e "${CYAN}🔍 Auto-detecting module directories...${NC}"
modules=()
discover_firmware_modules

if ((${#modules[@]} == 0)); then
  echo -e "${RED}❌ No modules found! Make sure firmware directories contain platformio.ini files.${NC}" >&2
  exit 1
fi

IFS=', '
echo -e "${GREEN}📋 Detected ${#modules[@]} modules: ${modules[*]}${NC}"
unset IFS

if [[ ! -d "$firmwareDir" ]]; then
  mkdir -p "$firmwareDir"
  echo -e "${GREEN}📁 Created firmware directory${NC}"
fi

echo -e "${YELLOW}🧹 Cleaning firmware directory (excluding debug/test modules)...${NC}"
if [[ -d "$firmwareDir" ]]; then
  find "$firmwareDir" -maxdepth 1 -type f -name '*.uf2' ! -name '*can_debug*' ! -name '*can_test*' -delete 2>/dev/null || true
fi

successCount=0
failCount=0

for module in "${modules[@]}"; do
  leaf=$(leaf_name "$module")
  echo ""
  echo -e "${YELLOW}🔨 Building ${module} ...${NC}"

  if [[ ! -d "$module" ]]; then
    echo -e "${RED}❌ Module directory '${module}' not found${NC}" >&2
    ((failCount++)) || true
    continue
  fi

  build_output=$(cd "$module" && pio run 2>&1) || true
  build_status=$?

  if [[ $build_status -eq 0 ]]; then
    echo -e "${GREEN}✅ ${leaf} build successful${NC}"
    ((successCount++)) || true

    up=$(relative_up "$module")
    found_any=0
    while IFS= read -r uf2; do
      [[ -z "$uf2" ]] && continue
      [[ -f "$uf2" ]] || continue
      found_any=1
      base=$(basename "$uf2")
      dest="${firmwareDir}/${leaf}_${base}"
      cp -f "$uf2" "$dest"
      echo -e "${GREEN}📦 Copied ${base} to ${leaf}_${base}${NC}"
    done < <(find "${up}${buildDir}/${leaf}" -type f -name '*.uf2' 2>/dev/null)

    if [[ $found_any -eq 0 ]]; then
      echo -e "${YELLOW}⚠️  No uf2 files found for ${leaf}${NC}"
    fi
  else
    echo -e "${RED}❌ ${leaf} build failed${NC}" >&2
    echo -e "${RED}${build_output}${NC}" >&2
    ((failCount++)) || true
  fi
done

echo ""
echo -e "${CYAN}📊 Build Summary:${NC}"
echo -e "${CYAN}=================${NC}"
echo -e "${GREEN}✅ Successful builds: ${successCount}${NC}"
echo -e "${RED}❌ Failed builds: ${failCount}${NC}"

if ((successCount > 0)); then
  echo ""
  echo -e "${GREEN}📁 Firmware files available in '${firmwareDir}' directory:${NC}"
  shopt -s nullglob
  for f in "${firmwareDir}"/*.uf2; do
    [[ -f "$f" ]] || continue
    sz=$(file_size_kb "$f")
    echo -e "${GRAY}  • $(basename "$f") (${sz} KB)${NC}"
  done
  shopt -u nullglob
fi

echo ""
if ((failCount == 0)); then
  echo -e "${GREEN}🎉 All modules built successfully!${NC}"
else
  echo -e "${YELLOW}⚠️  Some modules failed to build. Check the output above for details.${NC}"
fi
