// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/content_injection/platforms/pinterest_rules.h"

namespace content_injection {

namespace {

// pinterest.com — hide the algorithmic home feed, masonry grid, closeup feed,
// related pins, floating footer, and tablist. Preserves navigation sidebar
// (VerticalNavContent) and header (HeaderContent).
//
// Uses MutationObserver for dynamically injected elements and patches
// pushState/replaceState for SPA navigation.
constexpr std::string_view kPinterestJS = R"JS(
(() => {
  var old = document.getElementById('pdb-style');
  if (old) old.parentNode.removeChild(old);
  var nav = document.getElementById('VerticalNavContent');
  if (nav) nav.style.removeProperty('display');
  var header = document.getElementById('HeaderContent');
  if (header) header.style.removeProperty('display');
  var SELECTORS = [
    '[data-test-id="default-tab"]',
    '[data-test-id="search-story-suggestions-container"]',
    '[data-test-id="search-suggestion-curated-board-bubble"]',
    '[data-test-id="carousel-bubble-wrapper-slp_immersive_header"]',
    '[data-test-id="homefeed-feed"]',
    '[data-test-id="masonry-container"]',
    '[data-test-id="max-width-container"]',
    '[data-test-id="closeup-feed"]',
    '[data-test-id="related-pins-grid"]',
    '[data-test-id="floating-footer"]',
    '[data-test-id="more-ideas-tabs"]',
    '[data-root-margin="more-ideas-tabs"]',
  ];
  function hideAll() {
    const tablist = document.querySelector('[data-root-margin="more-ideas-tabs"] [role="tablist"]') ||
                    document.querySelector('[data-test-id="homefeed-feed"] [role="tablist"]') ||
                    document.querySelector('[role="tablist"]:has(#homefeed)');
    if (tablist) tablist.remove();
    const quotesBanner = document.querySelector('a[href*="/school-quotes-funny/"]');
    if (quotesBanner) quotesBanner.remove();
    SELECTORS.forEach(function(sel) {
      document.querySelectorAll(sel).forEach(function(el) {
        el.style.setProperty('display', 'none', 'important');
      });
    });
    var n = document.getElementById('VerticalNavContent');
    if (n) n.style.removeProperty('display');
    var h = document.getElementById('HeaderContent');
    if (h) h.style.removeProperty('display');
  }
  hideAll();
  var observer = new MutationObserver(hideAll);
  observer.observe(document.body, { childList: true, subtree: true });
  ['pushState','replaceState'].forEach(function(m) {
    var orig = history[m];
    history[m] = function() {
      var r = orig.apply(this, arguments);
      setTimeout(hideAll, 100);
      setTimeout(hideAll, 500);
      return r;
    };
  });
  window.addEventListener('popstate', function() { setTimeout(hideAll, 100); });
})();
)JS";

constexpr InjectionRule kRules[] = {
    {"pinterest.com", "", InjectionType::kJavaScript, kPinterestJS},
};

}  // namespace

base::span<const InjectionRule> GetPinterestInjectionRules() {
  return kRules;
}

}  // namespace content_injection
