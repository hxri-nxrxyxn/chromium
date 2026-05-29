// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/navigation_policy/platform_rules/linkedin_block_rules.h"

namespace {

// linkedin.com — block the Reels video feed.
//
// /videos/reels  → LinkedIn's dedicated Reels section
//
// The main feed (/feed/), profile pages (/in/), and job listings are
// handled by the content injection layer (CSS suppression), not hard-blocked.
constexpr BlockRule kLinkedInPrefixRules[] = {
    {"linkedin.com", "/videos/reels"},
};

}  // namespace

base::span<const BlockRule> GetLinkedInPrefixRules() {
  return kLinkedInPrefixRules;
}

base::span<const RegexBlockRule> GetLinkedInRegexRules() {
  return {};
}
