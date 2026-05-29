// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/navigation_policy/platform_rules/facebook_block_rules.h"

namespace {

// facebook.com — block Reels and Watch feeds.
//
// /reels/<id>  → algorithmic Reels feed (plural form)
// /reel/<id>   → individual Reel permalink (singular variant)
// /watch/      → Watch tab (primarily Reels/video feed)
//
// The regular home feed (/), Groups, Marketplace, etc. are unaffected.
constexpr BlockRule kFacebookPrefixRules[] = {
    {"facebook.com", "/reels"},  // facebook.com/reels/<id>
    {"facebook.com", "/reel"},   // facebook.com/reel/<id>  (singular variant)
    {"facebook.com", "/watch"},  // facebook.com/watch/     (Watch feed)
};

}  // namespace

base::span<const BlockRule> GetFacebookPrefixRules() {
  return kFacebookPrefixRules;
}

base::span<const RegexBlockRule> GetFacebookRegexRules() {
  return {};
}
