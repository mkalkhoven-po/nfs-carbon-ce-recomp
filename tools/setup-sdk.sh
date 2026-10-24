#!/usr/bin/env bash
# Prepare sdk/rexglue-sdk after cloning: fetch submodules, replace git symlink
# placeholders (Windows checks them out as text files), apply local patches.
#
# Usage: bash tools/setup-sdk.sh
set -euo pipefail

root="$(cd "$(dirname "$0")/.." && pwd)"
sdk="$root/sdk/rexglue-sdk"

git -C "$root" submodule update --init sdk/rexglue-sdk
git -C "$sdk" submodule update --init --recursive --depth 1 --jobs 8

# Symlinks stored in the SDK's submodules (mode 120000)
links=$(cd "$sdk" && git submodule foreach --recursive --quiet \
  'git ls-files -s | awk "\$1==\"120000\" {print \$4}" | sed "s|^|$displaypath/|"')
for link in $links; do
  path="$sdk/$link"
  [ -f "$path" ] || continue  # already a real symlink/dir
  target="$(dirname "$path")/$(cat "$path")"
  if [ -f "$target" ]; then
    cp "$target" "$path"
  elif [ -d "$target" ]; then
    rm "$path" && cp -r "$target" "$path"
  fi  # missing targets are macOS-only (MoltenVK) and can be ignored
done

for patch in "$root"/patches/rexglue-sdk/*.patch; do
  if git -C "$sdk" apply --check "$patch" 2>/dev/null; then
    git -C "$sdk" apply "$patch"
    echo "applied $(basename "$patch")"
  else
    echo "skipped $(basename "$patch") (already applied or conflicts)"
  fi
done
