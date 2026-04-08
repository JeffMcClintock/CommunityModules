#!/usr/bin/env python3
"""
generate_wxs.py  <source-dir>  <output.wxs>

Walks <source-dir> and produces a WiX v4 fragment containing:
  - A ComponentGroup "ModuleFiles" (referenced by community_modules.wxs)
  - A DirectoryRef "INSTALLDIR" with the full nested directory / component tree

This replaces `wix harvest dir` which is only available in WiX v5+.
"""

import os
import sys
from pathlib import Path


def main():
    if len(sys.argv) != 3:
        print(f"Usage: {sys.argv[0]} <source-dir> <output.wxs>")
        sys.exit(1)

    source_dir = Path(sys.argv[1]).resolve()
    output_path = sys.argv[2]

    dir_counter = [0]
    comp_counter = [0]
    component_ids = []
    dir_lines = []

    def walk(dir_path, indent):
        for entry in sorted(os.listdir(dir_path)):
            full = dir_path / entry
            if full.is_dir():
                dir_counter[0] += 1
                d_id = f"Dir_{dir_counter[0]:04d}"
                dir_lines.append(f'{indent}<Directory Id="{d_id}" Name="{entry}">')
                walk(full, indent + "  ")
                dir_lines.append(f"{indent}</Directory>")
            elif full.is_file():
                comp_counter[0] += 1
                n = comp_counter[0]
                c_id = f"Comp_{n:04d}"
                f_id = f"File_{n:04d}"
                component_ids.append(c_id)
                dir_lines.append(f'{indent}<Component Id="{c_id}" Guid="*">')
                dir_lines.append(f'{indent}  <File Id="{f_id}" Source="{full}" />')
                dir_lines.append(f"{indent}</Component>")

    walk(source_dir, "      ")

    lines = [
        '<?xml version="1.0" encoding="UTF-8"?>',
        '<Wix xmlns="http://wixtoolset.org/schemas/v4/wxs">',
        "",
        "  <!-- Component list referenced by community_modules.wxs -->",
        "  <Fragment>",
        '    <ComponentGroup Id="ModuleFiles">',
        *[f'      <ComponentRef Id="{cid}" />' for cid in component_ids],
        "    </ComponentGroup>",
        "  </Fragment>",
        "",
        "  <!-- Full directory tree rooted at INSTALLDIR -->",
        "  <Fragment>",
        '    <DirectoryRef Id="INSTALLDIR">',
        *dir_lines,
        "    </DirectoryRef>",
        "  </Fragment>",
        "",
        "</Wix>",
    ]

    Path(output_path).write_text("\n".join(lines), encoding="utf-8")
    print(
        f"Generated {output_path}: "
        f"{comp_counter[0]} files across {dir_counter[0]} directories."
    )


if __name__ == "__main__":
    main()
