#!/usr/bin/env python3
"""Apply custom patches to a Chromium checkout.

Usage:
    apply-patches.py /path/to/chromium/src /path/to/patches/dir

If a `patches/` directory containing `.patch` files exists in your repository,
this script copies only the new files verbatim and applies the standard patches
using `git apply`.

Otherwise, it falls back to copying all files verbatim and performing surgical
string replacements on registration files in the checkout.
"""

import argparse
import glob
import os
import shutil
import subprocess
import sys

def die(message: str) -> None:
    """Print an error message to stderr and exit with code 1."""
    print(f"error: {message}", file=sys.stderr)
    sys.exit(1)

def read_file(path: str) -> str:
    """Read *path* and return its contents, or die with a helpful message."""
    try:
        with open(path, encoding="utf-8") as fh:
            return fh.read()
    except FileNotFoundError:
        die(f"file not found: {path!r}\n"
            "  Is CHROMIUM_SRC pointing at the correct checkout?")
    except PermissionError:
        die(f"permission denied reading {path!r}")

def write_file(path: str, content: str) -> None:
    """Write *content* to *path*, or die with a helpful message."""
    try:
        with open(path, "w", encoding="utf-8") as fh:
            fh.write(content)
    except PermissionError:
        die(f"permission denied writing {path!r}")

def copy_new_files_only(source_files_dir: str, chromium_src: str) -> None:
    """Recursively copy only brand new files into *chromium_src* (skips files that exist)."""
    print("━━━ Copying brand-new source files ━━━")
    for root, _dirs, files in os.walk(source_files_dir):
        for fname in files:
            src_path = os.path.join(root, fname)
            rel_path = os.path.relpath(src_path, source_files_dir)
            dest_path = os.path.join(chromium_src, rel_path)
            
            # Only copy if the file does not already exist in Chromium checkout
            if not os.path.exists(dest_path):
                os.makedirs(os.path.dirname(dest_path), exist_ok=True)
                shutil.copy2(src_path, dest_path)
                print(f"  ✓ Copied new file: {rel_path}")

def copy_all_source_files(source_files_dir: str, chromium_src: str) -> None:
    """Recursively copy every file under *source_files_dir* into *chromium_src*."""
    print("━━━ Copying all source files verbatim (Legacy Mode) ━━━")
    for root, _dirs, files in os.walk(source_files_dir):
        for fname in files:
            src_path = os.path.join(root, fname)
            rel_path = os.path.relpath(src_path, source_files_dir)
            dest_path = os.path.join(chromium_src, rel_path)
            os.makedirs(os.path.dirname(dest_path), exist_ok=True)
            shutil.copy2(src_path, dest_path)
            print(f"  ✓ Copied  {rel_path}")

def apply_git_patches(patches_dir: str, chromium_src: str) -> None:
    """Apply all standard git patch files inside patches_dir using git apply."""
    print("\n━━━ Applying standard git patches ━━━")
    patch_files = sorted(glob.glob(os.path.join(patches_dir, "*.patch")))
    
    if not patch_files:
        print("  - No patch files found in patches/ directory.")
        return

    for patch in patch_files:
        patch_name = os.path.basename(patch)
        print(f"  Applying {patch_name}...")
        try:
            subprocess.run(
                ["git", "apply", "--whitespace=nowarn", "--recount", patch],
                cwd=chromium_src,
                check=True,
                capture_output=True,
                text=True
            )
            print(f"    ✓ Applied successfully")
        except subprocess.CalledProcessError as e:
            print(f"    ❌ Failed to apply {patch_name}")
            print(f"    Error: {e.stderr}")
            print("    Attempting to apply with reject files (--reject)...")
            try:
                subprocess.run(
                    ["git", "apply", "--whitespace=nowarn", "--recount", "--reject", patch],
                    cwd=chromium_src,
                    check=True,
                    capture_output=True,
                    text=True
                )
                print(f"    ✓ Applied with rejects (.rej files created)")
            except subprocess.CalledProcessError as err_reject:
                die(f"Could not apply patch {patch_name} even with rejects.\n"
                    f"Error: {err_reject.stderr}")

def patch_throttles_file(chromium_src: str) -> None:
    """Add includes and throttle/tab-helper registration (Legacy Mode)."""
    print("\n━━━ Patching navigation throttles file (Legacy Mode) ━━━")
    rel = "chrome/browser/chrome_content_browser_client_navigation_throttles.cc"
    path = os.path.join(chromium_src, rel)
    content = read_file(path)
    modified = False

    include_injection = '#include "chrome/browser/content_injection/content_injection_manager.h"'
    if include_injection in content:
        print("  - content_injection include already present, skipping")
    else:
        anchor = '#include "chrome/browser/data_sharing/data_sharing_navigation_throttle.h"'
        if anchor not in content:
            die(f"anchor not found in {rel!r}:\n  {anchor!r}")
        content = content.replace(anchor, anchor + "\n" + include_injection)
        modified = True
        print("  ✓ Added content_injection #include")

    include_blocker = '#include "chrome/browser/navigation_policy/shorts_reels_blocker.h"'
    if include_blocker in content:
        print("  - shorts_reels_blocker include already present, skipping")
    else:
        content = content.replace(include_injection, include_injection + "\n" + include_blocker)
        modified = True
        print("  ✓ Added shorts_reels_blocker #include")

    registration_sentinel = "ShortsReelsBlockerThrottle::CreateForNavigation"
    if registration_sentinel in content:
        print("  - Throttle registration already present, skipping")
    else:
        anchor = "page_load_metrics::MetricsNavigationThrottle::CreateAndAdd(registry);"
        if anchor not in content:
            die(f"anchor not found in {rel!r}:\n  {anchor!r}")
        insert = (
            "    // Block short-form video feed URLs (YouTube Shorts, Instagram Reels,\n"
            "    // Facebook Reels/Watch, Reddit Reels, X Reels, LinkedIn Reels).\n"
            "    // The TabHelper is attached here so it persists for same-document SPA\n"
            "    // navigations that bypass the throttle entirely.\n"
            "    registry.AddThrottle(\n"
            "        ShortsReelsBlockerThrottle::CreateForNavigation(registry));\n"
            "\n"
            "    content::WebContents* web_contents = handle.GetWebContents();\n"
            "    if (web_contents) {\n"
            "      ShortsReelsBlockerTabHelper::CreateForWebContents(web_contents);\n"
            "      content_injection::ContentInjectionManager::CreateForWebContents(\n"
            "          web_contents);\n"
            "    }\n"
        )
        content = content.replace(anchor, anchor + "\n" + insert)
        modified = True
        print("  ✓ Added throttle + tab-helper + injection-manager registration")

    if modified:
        write_file(path, content)

def patch_build_gn(chromium_src: str) -> None:
    """Add source_set deps to chrome/browser/BUILD.gn (Legacy Mode)."""
    print("\n━━━ Patching chrome/browser/BUILD.gn (Legacy Mode) ━━━")
    path = os.path.join(chromium_src, "chrome/browser/BUILD.gn")
    content = read_file(path)
    gn_modified = False

    blocker_dep = '"//chrome/browser/navigation_policy:shorts_reels_blocker"'
    injection_dep = '"//chrome/browser/content_injection:content_injection"'

    if blocker_dep in content and injection_dep in content:
        print("  - Both source_set deps already present, skipping")
        return

    anchor = '"//chrome/browser/navigation_predictor",'
    if anchor not in content:
        die(f"anchor not found in chrome/browser/BUILD.gn:\n  {anchor!r}")

    insert_parts = []
    if blocker_dep not in content:
        insert_parts.append(f"\n    {blocker_dep},")
    if injection_dep not in content:
        insert_parts.append(f"\n    {injection_dep},")

    if insert_parts:
        content = content.replace(anchor, anchor + "".join(insert_parts))
        gn_modified = True
        for dep in (blocker_dep, injection_dep):
            if dep in "".join(insert_parts):
                print(f"  ✓ Added {dep} to deps")

    if gn_modified:
        write_file(path, content)

def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    parser.add_argument(
        "chromium_src",
        metavar="CHROMIUM_SRC",
        help="Absolute path to the Chromium src/ checkout directory.",
    )
    parser.add_argument(
        "patches_dir",
        metavar="PATCHES_DIR",
        help="Path to this patches repository.",
    )
    return parser.parse_args()

def validate_args(args: argparse.Namespace) -> None:
    if not os.path.isdir(args.chromium_src):
        die(f"CHROMIUM_SRC is not a directory: {args.chromium_src!r}")
    if not os.path.isdir(args.patches_dir):
        die(f"PATCHES_DIR is not a directory: {args.patches_dir!r}")

    source_files = os.path.join(args.patches_dir, "source-files")
    if not os.path.isdir(source_files):
        die(f"source-files/ directory not found inside PATCHES_DIR: {source_files!r}")

def main() -> None:
    args = parse_args()
    validate_args(args)

    source_files_dir = os.path.join(args.patches_dir, "source-files")
    patches_folder = os.path.join(args.patches_dir, "patches")

    # If the patches directory exists and contains at least one .patch file, use Patch Mode
    has_patches = os.path.isdir(patches_folder) and any(
        fname.endswith(".patch") for fname in os.listdir(patches_folder)
    )

    if has_patches:
        print("💡 Standard Patch Mode detected.")
        copy_new_files_only(source_files_dir, args.chromium_src)
        apply_git_patches(patches_folder, args.chromium_src)
    else:
        print("⚠️ Legacy Copy-and-Replace Mode detected (No .patch files found).")
        copy_all_source_files(source_files_dir, args.chromium_src)
        patch_throttles_file(args.chromium_src)
        patch_build_gn(args.chromium_src)

    print("\n━━━ All patches applied successfully ━━━")

if __name__ == "__main__":
    main()
