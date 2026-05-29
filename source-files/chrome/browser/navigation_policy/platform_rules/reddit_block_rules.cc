// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/navigation_policy/platform_rules/reddit_block_rules.h"

namespace {

// reddit.com — block the Reels feed section.
//
// /reels  → Reddit Reels feed
//
// Note: the mobile share short-links (/r/<sub>/s/<id>) cannot be expressed
// as a prefix because that would block all of /r/. They are handled by the
// regex rule below.
constexpr BlockRule kRedditPrefixRules[] = {
    {"reddit.com", "/reels"},
};

// reddit.com mobile share short-links: /r/<subreddit>/s/<id>
// These are share-link redirects that often lead to Reels/video content.
// Pattern: /r/ followed by a subreddit name, then /s/, then a share ID.
constexpr RegexBlockRule kRedditRegexRules[] = {
    {"reddit.com", R"(/r/[^/]+/s/[^/]+)"},
};

}  // namespace

base::span<const BlockRule> GetRedditPrefixRules() {
  return kRedditPrefixRules;
}

base::span<const RegexBlockRule> GetRedditRegexRules() {
  return kRedditRegexRules;
}
