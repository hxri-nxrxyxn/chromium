// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/content_injection/platforms/youtube_rules.h"

namespace content_injection {

namespace {

// youtube.com — hide the Shorts shelf on the home page and search results.
//
// Hard URL blocks (throttle layer):  /shorts/<id> and /shorts browse page
// CSS injection (this layer):        Shorts shelf row in the home feed
//
// The selector is keyed on the red Shorts SVG icon path fill value (#f03)
// so it targets the section header precisely without relying on fragile
// generated class names that change on every YouTube deploy.
constexpr std::string_view kYoutubeShortsShelfCSS = R"CSS(
  ytm-rich-section-renderer:has(span.yt-icon-shape svg path[fill="#f03"]) {
    display: none !important;
  }
)CSS";

constexpr InjectionRule kRules[] = {
    {"youtube.com", "", InjectionType::kCSS, kYoutubeShortsShelfCSS},
};

}  // namespace

base::span<const InjectionRule> GetYouTubeInjectionRules() {
  return kRules;
}

}  // namespace content_injection
