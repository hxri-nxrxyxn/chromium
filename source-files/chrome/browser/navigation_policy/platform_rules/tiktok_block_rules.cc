// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/navigation_policy/platform_rules/tiktok_block_rules.h"

namespace {

// tiktok.com — block the entire domain.
//
// TikTok's architecture is a pure short-form video feed with no "safe"
// sub-sections, so the whole domain is blocked with an empty path prefix.
// An empty path_prefix in PathMatchesPrefix() matches every URL on the domain.
constexpr BlockRule kTikTokPrefixRules[] = {
    {"tiktok.com", ""},  // "" → entire domain
};

}  // namespace

base::span<const BlockRule> GetTikTokPrefixRules() {
  return kTikTokPrefixRules;
}

base::span<const RegexBlockRule> GetTikTokRegexRules() {
  return {};
}
