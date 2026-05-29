// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// ---------------------------------------------------------------------------
// ALL PLATFORM BLOCK RULES — AGGREGATOR
//
// This file is the ONLY place that knows about all platforms.
// To add a new platform:
//   1. Create platform_rules/<name>_block_rules.h + .cc
//   2. Add its header #include below
//   3. Call its Get*Rules() functions in the two aggregation lambdas below
//   4. Add the new .cc to platform_rules/BUILD.gn
//
// No other files need to change.
// ---------------------------------------------------------------------------

#include "chrome/browser/navigation_policy/platform_rules/all_block_rules.h"

#include "base/no_destructor.h"
#include "chrome/browser/navigation_policy/platform_rules/facebook_block_rules.h"
#include "chrome/browser/navigation_policy/platform_rules/instagram_block_rules.h"
#include "chrome/browser/navigation_policy/platform_rules/linkedin_block_rules.h"
#include "chrome/browser/navigation_policy/platform_rules/reddit_block_rules.h"
#include "chrome/browser/navigation_policy/platform_rules/tiktok_block_rules.h"
#include "chrome/browser/navigation_policy/platform_rules/x_block_rules.h"
#include "chrome/browser/navigation_policy/platform_rules/youtube_block_rules.h"

namespace {

// Helper: append all elements from |src| into |dst|.
template <typename T>
void Append(std::vector<T>& dst, base::span<const T> src) {
  dst.insert(dst.end(), src.begin(), src.end());
}

}  // namespace

const std::vector<BlockRule>& GetAllPrefixBlockRules() {
  static const base::NoDestructor<std::vector<BlockRule>> kRules([] {
    std::vector<BlockRule> rules;
    // ── Add platforms in alphabetical order ──────────────────────────────────
    Append(rules, GetFacebookPrefixRules());
    Append(rules, GetInstagramPrefixRules());
    Append(rules, GetLinkedInPrefixRules());
    Append(rules, GetRedditPrefixRules());
    Append(rules, GetTikTokPrefixRules());
    Append(rules, GetXPrefixRules());
    Append(rules, GetYouTubePrefixRules());
    return rules;
  }());
  return *kRules;
}

const std::vector<RegexBlockRule>& GetAllRegexBlockRules() {
  static const base::NoDestructor<std::vector<RegexBlockRule>> kRules([] {
    std::vector<RegexBlockRule> rules;
    // ── Add platforms in alphabetical order ──────────────────────────────────
    Append(rules, GetFacebookRegexRules());
    Append(rules, GetInstagramRegexRules());
    Append(rules, GetLinkedInRegexRules());
    Append(rules, GetRedditRegexRules());
    Append(rules, GetTikTokRegexRules());
    Append(rules, GetXRegexRules());
    Append(rules, GetYouTubeRegexRules());
    return rules;
  }());
  return *kRules;
}
