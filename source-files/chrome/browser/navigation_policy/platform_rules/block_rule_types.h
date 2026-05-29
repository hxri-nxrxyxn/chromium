// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NAVIGATION_POLICY_PLATFORM_RULES_BLOCK_RULE_TYPES_H_
#define CHROME_BROWSER_NAVIGATION_POLICY_PLATFORM_RULES_BLOCK_RULE_TYPES_H_

#include <string_view>

// ---------------------------------------------------------------------------
// Shared rule structs consumed by ShortsReelsBlockerThrottle::CheckURL().
//
// These types are kept in their own header so each per-platform file can
// include only what it needs without pulling in the full blocker header.
// ---------------------------------------------------------------------------

// A domain + path-prefix rule.
//   registrable_domain  — matched with GURL::DomainIs() (eTLD+1, e.g. "youtube.com")
//   path_prefix         — lower-cased; "" means "entire domain"
//                         PathMatchesPrefix() enforces a component boundary so
//                         "/shorts" does NOT match "/shortsfilm".
struct BlockRule {
  std::string_view registrable_domain;
  std::string_view path_prefix;
};

// A regex rule for patterns that can't be expressed as a simple prefix.
// The pattern is matched against the lower-cased full URL path.
//   registrable_domain  — pre-filter; regex is only evaluated if domain matches.
//   pattern             — RE2 pattern string (compiled once on first use).
struct RegexBlockRule {
  std::string_view registrable_domain;
  std::string_view pattern;
};

#endif  // CHROME_BROWSER_NAVIGATION_POLICY_PLATFORM_RULES_BLOCK_RULE_TYPES_H_
