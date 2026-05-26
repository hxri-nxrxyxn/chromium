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

    # 2. Register throttle + tab helper + content injection manager in the
    #    navigation throttles file.
    throttles_file = os.path.join(
        chromium_src,
        "chrome/browser/chrome_content_browser_client_navigation_throttles.cc"
    )

    with open(throttles_file) as f:
        content = f.read()

    modified = False

    # 2a. Add shorts_reels_blocker include (content_injection is covered
    #     by the block header which includes the manager).
    include_blocker = (
        '#include "chrome/browser/navigation_policy/shorts_reels_blocker.h"'
    )
    include_injection = (
        '#include "chrome/browser/content_injection/content_injection_manager.h"'
    )

    if include_injection in content:
        print("  - content_injection include already present, skipping")
    else:
        anchor = '#include "chrome/browser/data_sharing/data_sharing_navigation_throttle.h"'
        content = content.replace(
            anchor,
            anchor + "\n" + include_injection
        )
        modified = True
        print("  ✓ Added content_injection #include")

    if include_blocker in content:
        print("  - shorts_reels_blocker include already present, skipping")
    else:
        anchor = '#include "chrome/browser/content_injection/content_injection_manager.h"'
        content = content.replace(
            anchor,
            anchor + "\n" + include_blocker
        )
        modified = True
        print("  ✓ Added shorts_reels_blocker #include")

    # 2b. Add throttle + tab-helper + injection manager registration
    registration = "ShortsReelsBlockerThrottle::CreateForNavigation"
    if registration in content:
        print("  - Throttle registration already present, skipping")
    else:
        anchor = "page_load_metrics::MetricsNavigationThrottle::CreateAndAdd(registry);"
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
        print("  ✓ Added throttle + tab-helper + injection-registration")

    if modified:
        with open(throttles_file, "w") as f:
            f.write(content)

    # 3. Add source_set deps to chrome/browser/BUILD.gn
    build_gn = os.path.join(chromium_src, "chrome/browser/BUILD.gn")

    with open(build_gn) as f:
        gn_content = f.read()

    gn_modified = False

    # Dep for shorts_reels_blocker source_set (handles //base, //content/public/browser, //third_party/re2, //ui/base, //url)
    blocker_dep = '"//chrome/browser/navigation_policy:shorts_reels_blocker"'
    injection_dep = '"//chrome/browser/content_injection:content_injection"'

    if blocker_dep in gn_content and injection_dep in gn_content:
        print("  - Both source_set deps already present, skipping")
    else:
        # Add after the navigation_predictor dep (alphabetical)
        anchor = '"//chrome/browser/navigation_predictor",'
        insert = (
            f"\n    {blocker_dep},"
            f"\n    {injection_dep},"
        )
        if blocker_dep in gn_content:
            print(f"  - {blocker_dep} already present")
        else:
            gn_content = gn_content.replace(anchor, anchor + insert)
            gn_modified = True
            print(f"  ✓ Added {blocker_dep} and {injection_dep} to deps")

    if gn_modified:
        with open(build_gn, "w") as f:
            f.write(gn_content)

    print("━━━ Patches applied successfully ━━━")


if __name__ == "__main__":
    main()
