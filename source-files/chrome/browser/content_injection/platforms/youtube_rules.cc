// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/content_injection/platforms/youtube_rules.h"

namespace content_injection {

namespace {

// youtube.com — mobile YouTube blocker that hides all content except the
// header bar and pivot bar (navigation). Kills feeds, player pages, shorts
// container, and lazy-loaders.
//
// The :not() selectors preserve the minimal UI needed to search and navigate.
constexpr std::string_view kYouTubeJS = R"JS(
(function() {
  var old = document.getElementById('youtube-mobile-distraction-blocker-style');
  if (old) old.parentNode.removeChild(old);
  var css = '';
  css += 'ytm-app > *:not(#header-bar):not(ytm-pivot-bar-renderer):not(ytm-header-bar):not(header) { display: none !important; } ';
  css += 'body > *:not(ytm-app):not(#header-bar):not(ytm-pivot-bar-renderer) { display: none !important; } ';
  css += 'ytm-single-page-app-body, #content, .lazy-list, ytm-browse, ytm-watch, ytm-shorts, #shorts-container { display: none !important; } ';
  var style = document.createElement('style');
  style.id = 'youtube-mobile-distraction-blocker-style';
  style.type = 'text/css';
  if (style.styleSheet) { style.styleSheet.cssText = css; }
  else { style.appendChild(document.createTextNode(css)); }
  document.head.appendChild(style);
})();
)JS";

constexpr InjectionRule kRules[] = {
    {"youtube.com", "", InjectionType::kJavaScript, kYouTubeJS},
    {"youtu.be",    "", InjectionType::kJavaScript, kYouTubeJS},
};

}  // namespace

base::span<const InjectionRule> GetYouTubeInjectionRules() {
  return kRules;
}

}  // namespace content_injection
