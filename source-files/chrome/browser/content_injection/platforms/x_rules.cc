// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/content_injection/platforms/x_rules.h"

namespace content_injection {

namespace {

// x.com — comprehensive content blocker via CSS injection.
//
// Stripped elements:
//   - header[role="banner"]        — left navigation sidebar
//   - [aria-label*="Timeline:"]    — home + explore timeline feeds
//   - [role="tablist"]             — "For You"/"Following" tab bar
//   - [data-testid="ScrollSnap-List"] — mobile tab scroll
//   - [data-testid="sidebarColumn"] section — trending / who to follow
//   - [aria-label="Who to follow"], [aria-label="Trending"]
//   - a[href="/i/grok"]            — Grok link
//   - a[href="/jobs"]              — Jobs link
//   - a[href="/i/premium_sign_up"] — Premium upsell
//
// Preserved: search bar, compose button, thread pages, profile pages.
constexpr std::string_view kXJS = R"JS(
(function(){
  const style=document.createElement('style');
  style.textContent=
    'header[role="banner"],'+
    '[aria-label="Timeline: Your Home Timeline"],'+
    '[aria-label="Timeline: Home timeline"],'+
    '[aria-label="Timeline: Explore"],'+
    '[role="tablist"],'+
    '[data-testid="ScrollSnap-List"],'+
    '[data-testid="sidebarColumn"] section,'+
    '[aria-label="Who to follow"],'+
    '[aria-label="Relevant people"],'+
    '[aria-label="Trending"],'+
    '[aria-label="Timeline: Trending now"],'+
    'a[href="/i/grok"],'+
    '[data-testid="AppTabBar_Grok_Link"],'+
    'a[href="/jobs"],'+
    'a[href="/i/premium_sign_up"],'+
    '[aria-label="Premium"]'+
    '{display:none!important;}';
  document.head.appendChild(style);
})();
)JS";

constexpr InjectionRule kRules[] = {
    {"x.com", "", InjectionType::kJavaScript, kXJS},
};

}  // namespace

base::span<const InjectionRule> GetXInjectionRules() {
  return kRules;
}

}  // namespace content_injection
