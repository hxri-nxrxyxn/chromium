// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/navigation_policy/platform_rules/x_block_rules.h"

namespace {

// x.com — block the Reels/video feed section.
//
// /i/reels  → X's short-form video Reels feed
//
// The main home timeline (/home) and Explore (/explore) are left for the
// content injection layer, which uses MutationObserver to remove algorithmic
// content while preserving the rest of the page.
constexpr BlockRule kXPrefixRules[] = {
    {"x.com", "/i/reels"},
};

}  // namespace

base::span<const BlockRule> GetXPrefixRules() {
  return kXPrefixRules;
}

base::span<const RegexBlockRule> GetXRegexRules() {
  return {};
}
