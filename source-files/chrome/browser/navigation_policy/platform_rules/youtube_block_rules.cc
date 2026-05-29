// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/navigation_policy/platform_rules/youtube_block_rules.h"

namespace {

// youtube.com — block the Shorts feed.
// Individual /shorts/<id> URLs and the /shorts browse page are both blocked.
// The rest of YouTube (search, watch, channel pages) is unaffected.
constexpr BlockRule kYouTubePrefixRules[] = {
    {"youtube.com", "/shorts"},
};

}  // namespace

base::span<const BlockRule> GetYouTubePrefixRules() {
  return kYouTubePrefixRules;
}

base::span<const RegexBlockRule> GetYouTubeRegexRules() {
  return {};
}
