// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/navigation_policy/platform_rules/instagram_block_rules.h"

namespace {

// instagram.com — block the Reels browse feed only.
//
// /reels/…   → the algorithmic Reels feed (blocked)
//
// Profile videos like /username/reel/<id> start with the username segment,
// NOT "/reels", so they pass through and are not affected.
constexpr BlockRule kInstagramPrefixRules[] = {
    {"instagram.com", "/reels"},
};

}  // namespace

base::span<const BlockRule> GetInstagramPrefixRules() {
  return kInstagramPrefixRules;
}

base::span<const RegexBlockRule> GetInstagramRegexRules() {
  return {};
}
