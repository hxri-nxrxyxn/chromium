// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/content_injection/platforms/reddit_rules.h"

namespace content_injection {

namespace {

// reddit.com — hide the home feed and lock scroll on the root path.
// Subreddits, search, and thread pages are unaffected.
//
// SPA-aware: patches pushState/replaceState and listens for popstate to
// re-apply the scroll lock on soft navigation.
constexpr std::string_view kRedditJS = R"JS(
(() => {
  var old = document.getElementById('reddit-style');
  if (old) old.parentNode.removeChild(old);
  const style = document.createElement('style');
  style.id = 'reddit-style';
  style.textContent =
    'shreddit-feed, shreddit-feed-page-loading, ' +
    'faceplate-loader[name*="HomeFeed"], ' +
    'suspense-placeholder[name="HomeFeed"] ' +
    '{ display: none !important; }';
  document.head.appendChild(style);

  function applyScrollLock() {
    document.documentElement.style.overflowY =
      (location.pathname === '/') ? 'hidden' : '';
  }
  applyScrollLock();

  ['pushState','replaceState'].forEach(m => {
    const orig = history[m];
    history[m] = function(...args) {
      const r = orig.apply(this, args);
      applyScrollLock();
      return r;
    };
  });
  window.addEventListener('popstate', applyScrollLock);
})();
)JS";

constexpr InjectionRule kRules[] = {
    {"reddit.com", "", InjectionType::kJavaScript, kRedditJS},
};

}  // namespace

base::span<const InjectionRule> GetRedditInjectionRules() {
  return kRules;
}

}  // namespace content_injection
