// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// ---------------------------------------------------------------------------
// ALL PLATFORM INJECTION RULES — AGGREGATOR
//
// This file aggregates all platform-specific injection rules.
// To add a new platform:
//   1. Create platforms/<name>_rules.h + .cc
//   2. Add its header #include below
//   3. Call its Get<Name>InjectionRules() function in the lambda below
//   4. Add the new .cc to platforms/BUILD.gn
// ---------------------------------------------------------------------------

#include "chrome/browser/content_injection/content_injection_rules.h"

#include <vector>

#include "base/no_destructor.h"
#include "chrome/browser/content_injection/platforms/facebook_rules.h"
#include "chrome/browser/content_injection/platforms/instagram_rules.h"
#include "chrome/browser/content_injection/platforms/linkedin_rules.h"
#include "chrome/browser/content_injection/platforms/reddit_rules.h"
#include "chrome/browser/content_injection/platforms/tumblr_rules.h"
#include "chrome/browser/content_injection/platforms/x_rules.h"
#include "chrome/browser/content_injection/platforms/youtube_rules.h"

namespace content_injection {

namespace {

// Helper: append all elements from |src| into |dst|.
template <typename T>
void Append(std::vector<T>& dst, base::span<const T> src) {
  dst.insert(dst.end(), src.begin(), src.end());
}

}  // namespace

base::span<const InjectionRule> GetInjectionRules() {
  static const base::NoDestructor<std::vector<InjectionRule>> kRules([] {
    std::vector<InjectionRule> rules;
    // ── Add platforms in alphabetical order ──────────────────────────────────
    Append(rules, GetFacebookInjectionRules());
    Append(rules, GetInstagramInjectionRules());
    Append(rules, GetLinkedInInjectionRules());
    Append(rules, GetRedditInjectionRules());
    Append(rules, GetTumblrInjectionRules());
    Append(rules, GetXInjectionRules());
    Append(rules, GetYouTubeInjectionRules());
    return rules;
  }());
  return *kRules;
}

}  // namespace content_injection
