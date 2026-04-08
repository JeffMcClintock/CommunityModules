#!/usr/bin/env python3
"""
merge_bundles.py  <mac-artifacts>  <win-artifacts>  <output-dir>

Merges macOS .sem/.gmpi bundles with their matching Windows DLLs to produce
universal bundles with this layout:

  PluginName.sem/
    Contents/
      Info.plist
      MacOS/
        PluginName          <- macOS universal binary (arm64 + x86_64)
      Resources/
        PluginName.xml
      x86_64-win/
        PluginName.sem      <- Windows PE32+ DLL
"""

import os
import sys
import shutil


BUNDLE_EXTS = ('.sem', '.gmpi')


def find_mac_bundles(root):
    """
    Walk the artifact tree and return all .sem/.gmpi bundle directories.
    Does not recurse into bundle directories themselves.
    """
    bundles = []
    for dirpath, dirnames, _ in os.walk(root):
        to_remove = []
        for d in dirnames:
            if not any(d.endswith(ext) for ext in BUNDLE_EXTS):
                continue
            full = os.path.join(dirpath, d)
            if os.path.isdir(os.path.join(full, 'Contents')):
                bundles.append(full)
                to_remove.append(d)   # don't recurse into the bundle
        for d in to_remove:
            dirnames.remove(d)
    return bundles


def find_win_dlls(root):
    """
    Walk the artifact tree and return a dict mapping filename -> full path
    for all Windows .sem/.gmpi DLL files.
    """
    dlls = {}
    for dirpath, _, filenames in os.walk(root):
        for f in filenames:
            if any(f.endswith(ext) for ext in BUNDLE_EXTS):
                # Last match wins - plugin names should be globally unique
                dlls[f] = os.path.join(dirpath, f)
    return dlls


def merge(mac_root, win_root, out_dir):
    os.makedirs(out_dir, exist_ok=True)

    mac_bundles = find_mac_bundles(mac_root)
    win_dlls = find_win_dlls(win_root)

    if not mac_bundles:
        print("ERROR: no macOS bundles found", file=sys.stderr)
        sys.exit(1)

    merged = 0
    mac_only = 0

    for bundle in mac_bundles:
        name = os.path.basename(bundle)          # e.g. "ADSR.sem"
        plugin = os.path.splitext(name)[0]       # e.g. "ADSR"
        out_bundle = os.path.join(out_dir, name)

        # Copy macOS bundle to output
        shutil.copytree(bundle, out_bundle, dirs_exist_ok=True)

        # Inject Windows DLL
        if name in win_dlls:
            win_dir = os.path.join(out_bundle, 'Contents', 'x86_64-win')
            os.makedirs(win_dir, exist_ok=True)
            shutil.copy2(win_dlls[name], os.path.join(win_dir, name))
            print(f"  [ok]      {name}")
            merged += 1
        else:
            print(f"  [mac only] {name}  (no matching Windows binary)")
            mac_only += 1

    print(f"\n{merged} universal bundles, {mac_only} macOS-only bundles.")


if __name__ == '__main__':
    if len(sys.argv) != 4:
        print(f"Usage: {sys.argv[0]} <mac-artifacts> <win-artifacts> <output-dir>")
        sys.exit(1)

    merge(sys.argv[1], sys.argv[2], sys.argv[3])
