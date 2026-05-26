#!/usr/bin/env python3
"""Apply custom patches to a Chromium checkout.
Usage: apply-patches.py /path/to/chromium/src /path/to/patches/dir
"""

import os
import sys
import shutil


def main():
    chromium_src = sys.argv[1]
    patches_dir = sys.argv[2]
    source_files = os.path.join(patches_dir, "source-files")

    print("━━━ Applying custom source files ━━━")

    # 1. Copy ALL new source files into the Chromium tree.
    #    The source-files directory mirrors the src/ layout.
    for root, dirs, files in os.walk(source_files):
        for f in files:
            src_path = os.path.join(root, f)
            rel_path = os.path.relpath(src_path, source_files)
            dest_path = os.path.join(chromium_src, rel_path)
            os.makedirs(os.path.dirname(dest_path), exist_ok=True)
            shutil.copy2(src_path, dest_path)
            print(f"  ✓ Added {rel_path}")

    # 2. Register throttle + tab helper + content injection manager
    throttles_file = os.path.join(
        chromium_src,
        "chrome/browser/chrome_content_browser_client_navigation_throttles.cc"
    )

    with open(throttles_file) as f:
        content = f.read()

    modified = False

    # 2a. Add includes for both modules
    includes = [
        '#include "chrome/browser/content_injection/content_injection_manager.h"',
        '#include "chrome/browser/navigation_policy/shorts_reels_blocker.h"',
    ]
    anchor = '#include "chrome/browser/data_sharing/data_sharing_navigation_throttle.h"'
    for inc in includes:
        if inc in content:
            print(f"  - Include already present: {inc}")
        else:
            content = content.replace(anchor, anchor + "\n" + inc)
            modified = True
            print(f"  ✓ Added include: {inc}")

    # 2b. Add throttle + tab-helper + injection-manager registration
    registration = "ShortsReelsBlockerThrottle::CreateForNavigation"
    if registration in content:
        print("  - Throttle registration already present, skipping")
    else:
        anchor = "page_load_metrics::MetricsNavigationThrottle::CreateAndAdd(registry);"
        insert = (
            "    // Block short-form video feed URLs (YouTube Shorts, Instagram Reels,\n"
            "    // Facebook Reels/Watch, Reddit Reels, X Reels, LinkedIn Reels).\n"
            "    // The TabHelper and injection manager are attached here so they\n"
            "    // persist for same-document SPA navigations.\n"
            "    registry.AddThrottle(\n"
            "        ShortsReelsBlockerThrottle::CreateForNavigation(registry));\n"
            "\n"
            "    content::WebContents* wc = handle.GetWebContents();\n"
            "    if (wc) {\n"
            "      ShortsReelsBlockerTabHelper::CreateForWebContents(wc);\n"
            "      content_injection::ContentInjectionManager::CreateForWebContents(wc);\n"
            "    }\n"
        )
        content = content.replace(anchor, anchor + "\n" + insert)
        modified = True
        print("  ✓ Added throttle + tab-helper + injection registration")

    if modified:
        with open(throttles_file, "w") as f:
            f.write(content)

    # 3. Add source_set deps to chrome/browser/BUILD.gn
    build_gn = os.path.join(chromium_src, "chrome/browser/BUILD.gn")

    with open(build_gn) as f:
        gn_content = f.read()

    gn_modified = False

    deps_to_add = [
        '"//chrome/browser/navigation_policy:shorts_reels_blocker"',
        '"//chrome/browser/content_injection:content_injection"',
    ]
    anchor = '"//chrome/browser/navigation_predictor",'
    insert = "\n    " + ",\n    ".join(deps_to_add) + ","
    for dep in deps_to_add:
        if dep in gn_content:
            print(f"  - Dep already present: {dep}")
        else:
            gn_content = gn_content.replace(anchor, anchor + insert)
            gn_modified = True
            print(f"  ✓ Added dep: {dep}")

    if gn_modified:
        with open(build_gn, "w") as f:
            f.write(gn_content)

    print("━━━ Patches applied successfully ━━━")


if __name__ == "__main__":
    main()
