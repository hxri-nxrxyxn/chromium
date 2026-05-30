// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/content_injection/platforms/instagram_rules.h"

namespace content_injection {

namespace {

// instagram.com — route-aware JS that hides the feed/stories/explore grid,
// preserves search/headers, unloads video assets, and handles SPA navigation.
//
// Three route modes:
//   /          → hide stories tray + main feed, pause/unload videos
//   /explore   → hide explore grid children (preserves search/header)
//   /anything  → cleanup/restore all hidden elements
//
// SPA-aware: patches pushState/replaceState and listens for popstate so
// the correct blocking mode is applied on every soft navigation.
constexpr std::string_view kInstagramJS = R"JS(
(function() {
  function applyBlock() {
    let path = window.location.pathname;
    if (path === '/' || path === '') {
      cleanupExploreBlock();
      applyHomeBlock();
    } else if (path.startsWith('/explore')) {
      cleanupHomeBlock();
      applyExploreBlock();
    } else {
      cleanupExploreBlock();
      cleanupHomeBlock();
    }
  }

  function applyHomeBlock() {
    const storyTray = document.querySelector('[data-pagelet="story_tray"]') ||
                      document.querySelector('a[href^="/stories/"]')?.closest('div');
    if (storyTray && !storyTray.hidden) {
      storyTray.setAttribute('data-org-class', storyTray.className);
      storyTray.className = '';
      storyTray.hidden = true;
    }
    const mainEl = document.querySelector('main') || document.querySelector('div[role="main"]');
    if (mainEl && !mainEl.hasAttribute('data-focus-blocked-home')) {
      mainEl.setAttribute('data-focus-blocked-home', 'true');
      mainEl.setAttribute('data-org-class-main', mainEl.className);
      mainEl.className = '';
      mainEl.hidden = true;
      mainEl.querySelectorAll('video').forEach(v => {
        try { v.pause(); v.removeAttribute('src');
              v.querySelectorAll('source').forEach(s => s.removeAttribute('src'));
              v.load(); } catch(e) {}
      });
    }
    document.querySelectorAll(
      'svg[aria-label="Loading..."], [role="progressbar"], ' +
      'div[aria-label="Loading..."], [data-visualcompletion="loading-state"]'
    ).forEach(sp => { sp.hidden = true; });
  }

  function cleanupHomeBlock() {
    const storyTray = document.querySelector('[data-pagelet="story_tray"]') ||
                      document.querySelector('a[href^="/stories/"]')?.closest('div');
    if (storyTray && storyTray.hidden) {
      if (storyTray.hasAttribute('data-org-class')) {
        storyTray.className = storyTray.getAttribute('data-org-class');
        storyTray.removeAttribute('data-org-class');
      }
      storyTray.hidden = false;
    }
    const mainEl = document.querySelector('main') || document.querySelector('div[role="main"]');
    if (mainEl && mainEl.hasAttribute('data-focus-blocked-home')) {
      mainEl.removeAttribute('data-focus-blocked-home');
      if (mainEl.hasAttribute('data-org-class-main')) {
        mainEl.className = mainEl.getAttribute('data-org-class-main');
        mainEl.removeAttribute('data-org-class-main');
      }
      mainEl.hidden = false;
    }
  }

  function applyExploreBlock() {
    const mainEl = document.querySelector('main') || document.querySelector('div[role="main"]');
    if (mainEl && !mainEl.hasAttribute('data-focus-blocked')) {
      mainEl.setAttribute('data-focus-blocked', 'true');
      mainEl.querySelectorAll('video').forEach(v => {
        try { v.pause(); v.removeAttribute('src');
              v.querySelectorAll('source').forEach(s => s.removeAttribute('src'));
              v.load(); } catch(e) {}
      });
      Array.from(mainEl.children).forEach(child => {
        if (child.querySelector('input') || child.querySelector('[role="search"]') ||
            child.tagName === 'HEADER') return;
        if (!child.hasAttribute('data-org-class'))
          child.setAttribute('data-org-class', child.className);
        child.hidden = true;
        child.className = '';
      });
    }
    document.querySelectorAll(
      'svg[aria-label="Loading..."], [role="progressbar"], ' +
      'div[aria-label="Loading..."], [data-visualcompletion="loading-state"]'
    ).forEach(sp => { sp.hidden = true; });
  }

  function cleanupExploreBlock() {
    const mainEl = document.querySelector('main') || document.querySelector('div[role="main"]');
    if (mainEl && mainEl.hasAttribute('data-focus-blocked')) {
      mainEl.removeAttribute('data-focus-blocked');
      Array.from(mainEl.children).forEach(child => {
        if (child.hasAttribute('data-org-class')) {
          child.className = child.getAttribute('data-org-class');
          child.removeAttribute('data-org-class');
        }
        child.hidden = false;
      });
    }
  }

  applyBlock();
  const observer = new MutationObserver(() => {
    observer.disconnect();
    try { applyBlock(); } catch(e) {}
    observer.observe(document.body, { childList: true, subtree: true });
  });
  observer.observe(document.body, { childList: true, subtree: true });

  ['pushState','replaceState'].forEach(m => {
    const orig = history[m];
    history[m] = function(...args) {
      const r = orig.apply(this, args);
      setTimeout(applyBlock, 0);
      return r;
    };
  });
  window.addEventListener('popstate', () => setTimeout(applyBlock, 0));
})();
)JS";

constexpr InjectionRule kRules[] = {
    {"instagram.com", "", InjectionType::kJavaScript, kInstagramJS},
};

}  // namespace

base::span<const InjectionRule> GetInstagramInjectionRules() {
  return kRules;
}

}  // namespace content_injection
