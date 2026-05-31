#!/usr/bin/env python3
"""Generate standard .patch files by comparing source-files/ with a clean Chromium checkout.

Usage:
    python3 generate-patches.py /path/to/chromium/src /path/to/this/repo
"""

import argparse
import os
import subprocess
import shutil
import sys

def die(message: str) -> None:
    print(f"error: {message}", file=sys.stderr)
    sys.exit(1)

def run_git(cwd: str, args: list) -> str:
    """Run a git command in cwd and return stdout."""
    try:
        res = subprocess.run(
            ["git"] + args,
            cwd=cwd,
            capture_output=True,
            text=True,
            check=True
        )
        return res.stdout
    except subprocess.CalledProcessError as e:
        die(f"git command failed inside {cwd}: {' '.join(args)}\nError: {e.stderr}")

def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("chromium_src", help="Path to clean Chromium src/ checkout directory.")
    parser.add_argument("patches_dir", help="Path to this patches repository.")
    args = parser.parse_args()

    chromium_src = os.path.abspath(args.chromium_src)
    patches_dir = os.path.abspath(args.patches_dir)
    source_files_dir = os.path.join(patches_dir, "source-files")
    output_patches_dir = os.path.join(patches_dir, "patches")

    if not os.path.isdir(chromium_src):
        die(f"Chromium src directory not found: {chromium_src}")
    if not os.path.isdir(source_files_dir):
        die(f"source-files/ directory not found in patches repo: {source_files_dir}")

    os.makedirs(output_patches_dir, exist_ok=True)

    print("━━━ Generating patches against Chromium checkout ━━━")
    print(f"Checkout: {chromium_src}")
    print(f"Patches repo: {patches_dir}\n")

    # 1. Check if git is initialized in checkout
    if not os.path.exists(os.path.join(chromium_src, ".git")):
        die("Chromium src is not a git repository. Cannot generate patches.")

    # 2. Iterate through source-files/
    for root, _, files in os.walk(source_files_dir):
        for fname in files:
            src_path = os.path.join(root, fname)
            rel_path = os.path.relpath(src_path, source_files_dir)
            dest_path = os.path.join(chromium_src, rel_path)

            # If the file exists in Chromium checkout, it's Category A (overwritten)
            if os.path.exists(dest_path):
                print(f"Generating patch for overwritten file: {rel_path}")

                # Copy modified version to checkout
                shutil.copy2(src_path, dest_path)

                # Run git diff
                diff_out = run_git(chromium_src, ["diff", "--no-color", "--", rel_path])

                # Revert checkout file
                run_git(chromium_src, ["checkout", "--", rel_path])

                # Save patch file
                if diff_out.strip():
                    patch_name = rel_path.replace(os.sep, "_") + ".patch"
                    patch_path = os.path.join(output_patches_dir, patch_name)
                    with open(patch_path, "w", encoding="utf-8") as fh:
                        fh.write(diff_out)
                    print(f"  ✓ Saved patch: patches/{patch_name}")
                else:
                    print("  - No changes detected (files identical)")
            else:
                # Category C (New file) - does not need a patch, will be copied verbatim
                print(f"New file (will copy verbatim): {rel_path}")

    # 3. Handle Category B: Surgical Modifications
    # We apply the edits of apply-patches.py to the checkout, diff them, and revert.
    print("\n━━━ Generating patches for surgical integrations ━━━")
    sys.path.append(patches_dir)
    try:
        import importlib.util
        spec = importlib.util.spec_from_file_location("apply_patches", os.path.join(patches_dir, "apply-patches.py"))
        apply_patches = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(apply_patches)
    except Exception as e:
        die(f"Failed to load apply-patches.py: {e}")

    # Patch and diff Navigation Throttles Registration file
    throttles_rel = "chrome/browser/chrome_content_browser_client_navigation_throttles.cc"
    throttles_path = os.path.join(chromium_src, throttles_rel)
    if os.path.exists(throttles_path):
        print(f"Patching & diffing: {throttles_rel}")
        apply_patches.patch_throttles_file(chromium_src)
        diff_out = run_git(chromium_src, ["diff", "--no-color", "--", throttles_rel])
        run_git(chromium_src, ["checkout", "--", throttles_rel])

        if diff_out.strip():
            patch_name = throttles_rel.replace(os.sep, "_") + ".patch"
            with open(os.path.join(output_patches_dir, patch_name), "w", encoding="utf-8") as fh:
                fh.write(diff_out)
            print(f"  ✓ Saved patch: patches/{patch_name}")

    # Patch and diff chrome/browser/BUILD.gn
    build_rel = "chrome/browser/BUILD.gn"
    build_path = os.path.join(chromium_src, build_rel)
    if os.path.exists(build_path):
        print(f"Patching & diffing: {build_rel}")
        apply_patches.patch_build_gn(chromium_src)
        diff_out = run_git(chromium_src, ["diff", "--no-color", "--", build_rel])
        run_git(chromium_src, ["checkout", "--", build_rel])

        if diff_out.strip():
            patch_name = build_rel.replace(os.sep, "_") + ".patch"
            with open(os.path.join(output_patches_dir, patch_name), "w", encoding="utf-8") as fh:
                fh.write(diff_out)
            print(f"  ✓ Saved patch: patches/{patch_name}")

    print("\n━━━ All patches generated successfully ━━━")
    print(f"Look in: {output_patches_dir}")

if __name__ == "__main__":
    main()
