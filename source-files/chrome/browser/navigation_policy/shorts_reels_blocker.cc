// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/navigation_policy/shorts_reels_blocker.h"

#include <atomic>
#include <string_view>

#include "base/memory/ptr_util.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "chrome/common/webui_url_constants.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/navigation_throttle_registry.h"
#include "content/public/browser/web_contents.h"
#include "net/base/net_errors.h"
#include "third_party/re2/src/re2/re2.h"
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"

namespace {

// ---------------------------------------------------------------------------
// Block rule table
//
// Each entry is a (registrable_domain, lower-cased path prefix) pair.
// PathMatchesPrefix() enforces component-boundary matching, so "/reel" does
// NOT match "/reelfilm".
//
// To add a new rule: append a row here. No logic changes required.
// ---------------------------------------------------------------------------
struct BlockRule {
  std::string_view registrable_domain;
  std::string_view path_prefix;  // must be lower-cased; "" = entire domain
};

constexpr BlockRule kPrefixBlockRules[] = {
    // ── YouTube ──────────────────────────────────────────────────────────────
    {"youtube.com", "/shorts"},

    // ── Instagram ────────────────────────────────────────────────────────────
    // Blocks the Reels browse feed (/reels/…).
    // Does NOT block profile videos (/username/reel/…) — those start with the
    // username segment, not "/reels", so they pass through naturally.
    {"instagram.com", "/reels"},

    // ── Facebook ─────────────────────────────────────────────────────────────
    {"facebook.com", "/reels"},  // facebook.com/reels/<id>
    {"facebook.com", "/reel"},   // facebook.com/reel/<id>  (singular variant)
    {"facebook.com", "/watch"},  // facebook.com/watch/     (Watch feed)

    // ── Reddit ───────────────────────────────────────────────────────────────
    {"reddit.com", "/reels"},

    // ── X / Twitter ──────────────────────────────────────────────────────────
    {"x.com", "/i/reels"},

    // ── LinkedIn ─────────────────────────────────────────────────────────────
    {"linkedin.com", "/videos/reels"},
};

// ---------------------------------------------------------------------------
// Regex-based rules for patterns that can't be expressed as a simple prefix.
// Each pattern is matched against the full path (lower-cased).
// ---------------------------------------------------------------------------
struct RegexBlockRule {
  std::string_view registrable_domain;
  // RE2 pattern — compiled once on first use via function-local static.
  std::string_view pattern;
};

constexpr RegexBlockRule kRegexBlockRules[] = {
    // Reddit mobile share short-links: /r/<subreddit>/s/<id>
    // These can't be prefix-blocked without also blocking all of /r/.
    {"reddit.com", R"(/r/[^/]+/s/[^/]+)"},
};

// Returns true if the regex rule matches the given lower-cased path.
// Each RE2 is compiled once (Meyer's singleton) and reused.
bool MatchesRegexRule(const RegexBlockRule& rule, std::string_view path) {
  // RE2 objects are thread-safe after construction.
  // We heap-allocate and intentionally leak — same pattern Chromium uses
  // for long-lived compiled regexes (see base/strings/pattern.cc).
  static const auto* kCompiledPatterns = [] {
    auto* patterns =
        new std::vector<std::unique_ptr<RE2>>(std::size(kRegexBlockRules));
    for (size_t i = 0; i < std::size(kRegexBlockRules); ++i) {
      (*patterns)[i] =
          std::make_unique<RE2>(std::string(kRegexBlockRules[i].pattern));
    }
    return patterns;
  }();

  const size_t index =
      static_cast<size_t>(&rule - std::begin(kRegexBlockRules));
  return RE2::PartialMatch(path, *(*kCompiledPatterns)[index]);
}

// ---------------------------------------------------------------------------
// Session-wide block counter (atomic, not persisted across restarts).
// ---------------------------------------------------------------------------
std::atomic<int> g_block_count{0};

// ---------------------------------------------------------------------------
// Block page HTML builder
// ---------------------------------------------------------------------------
std::string BuildBlockPageHTML(int count) {
  std::string counter_text = base::StringPrintf(
      "blocked %d time%s this session",
      count, count == 1 ? "" : "s");

  return base::StrCat({R"BLOCK(<!doctype html>
<html>
<head>
<meta charset="utf-8">
<meta name="color-scheme" content="light dark">
<meta name="theme-color" content="#fff">
<meta name="viewport" content="width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no">
<title>Page Blocked</title>
<style>
body{
  --background-color:#fff;
  --error-code-color:var(--google-gray-700);
  --google-blue-300:rgb(138,180,248);
  --google-blue-600:rgb(26,115,232);
  --google-gray-500:rgb(154,160,166);
  --google-gray-50:rgb(248,249,250);
  --google-gray-600:rgb(128,134,139);
  --google-gray-700:rgb(95,99,104);
  --google-gray-900:rgb(32,33,36);
  --heading-color:var(--google-gray-900);
  --link-color:rgb(88,88,88);
  --secondary-button-border-color:var(--google-gray-500);
  --secondary-button-fill-color:#fff;
  --secondary-button-hover-border-color:var(--google-gray-600);
  --secondary-button-hover-fill-color:var(--google-gray-50);
  --secondary-button-text-color:var(--google-gray-700);
  --text-color:var(--google-gray-700);
  background:var(--background-color);
  color:var(--text-color);
  font-family:-apple-system,BlinkMacSystemFont,"Segoe UI",Roboto,Oxygen,Ubuntu,Cantarell,"Fira Sans","Droid Sans","Helvetica Neue",sans-serif;
  word-wrap:break-word;margin:0;padding:0
}
html{-webkit-text-size-adjust:100%;font-size:125%}
h1{color:var(--heading-color);font-size:1.6em;font-weight:normal;line-height:1.25em;margin-bottom:16px;margin-top:0;word-wrap:break-word}
h1 span{font-weight:500}
p{color:var(--text-color);font-size:1.1em;line-height:1.55;margin-top:8px}
.icon{background-repeat:no-repeat;background-size:100%;display:inline-block;height:72px;margin:0 0 40px;width:72px;-webkit-user-select:none}
.icon-blocked{background-image:url("data:image/svg+xml,%3Csvg xmlns='http://www.w3.org/2000/svg' width='72' height='72' viewBox='0 0 72 72'%3E%3Ccircle cx='36' cy='36' r='32' fill='%23ea4335'/%3E%3Crect x='22' y='32' width='28' height='8' rx='4' fill='%23fff'/%3E%3C/svg%3E")}
.error-code{color:var(--error-code-color);font-size:.8em;margin-top:12px;text-transform:lowercase}
.nav-wrapper{margin-top:51px}
.nav-wrapper::after{clear:both;content:'';display:table;width:100%}
.secondary-button{background:var(--secondary-button-fill-color);border:1px solid var(--secondary-button-border-color);border-radius:20px;box-sizing:border-box;color:var(--secondary-button-text-color);cursor:pointer;display:inline-block;font-size:.875em;padding:8px 16px;text-decoration:none;user-select:none}
.secondary-button:hover{background:var(--secondary-button-hover-fill-color);border-color:var(--secondary-button-hover-border-color)}
.interstitial-wrapper{box-sizing:border-box;font-size:1em;line-height:1.6em;margin:14vh auto 0;max-width:600px;width:100%;padding:0 24px}
#main-content{padding-bottom:40px}
@media(prefers-color-scheme:dark){
  body{
    --background-color:var(--google-gray-900);
    --error-code-color:var(--google-gray-500);
    --heading-color:var(--google-gray-500);
    --link-color:var(--google-blue-300);
    --secondary-button-border-color:var(--google-gray-700);
    --secondary-button-fill-color:var(--google-gray-900);
    --secondary-button-hover-fill-color:rgb(48,51,57);
    --secondary-button-text-color:var(--google-blue-300);
    --text-color:var(--google-gray-500)
  }
  .icon-blocked{background-image:url("data:image/svg+xml,%3Csvg xmlns='http://www.w3.org/2000/svg' width='72' height='72' viewBox='0 0 72 72'%3E%3Ccircle cx='36' cy='36' r='32' fill='%23f28b82'/%3E%3Crect x='22' y='32' width='28' height='8' rx='4' fill='%23202124'/%3E%3C/svg%3E")}
}
@media(max-width:700px){.interstitial-wrapper{padding:0 10%}}
@media(max-width:420px){
  .interstitial-wrapper{padding:0 5%;margin:7vh auto 12px;padding:0 24px}
  h1{font-size:1.5em;margin-bottom:8px}
  .icon{margin-bottom:5.69vh}
}
</style>
</head>
<body>
<div id="content">
  <div id="main-frame-error" class="interstitial-wrapper">
    <div id="main-content">
      <div class="icon icon-blocked"></div>
      <div id="main-message">
        <h1><span>This page was blocked</span></h1>
        <p>Short-form videos aren't available here. That was your call.</p>
        <div class="error-code">)BLOCK",
      counter_text,
      R"BLOCK(</div>
      </div>
      <div class="nav-wrapper">
        <a class="secondary-button" href="javascript:history.go(-1)">Back to previous page</a>
      </div>
    </div>
  </div>
</div>
</body>
</html>)BLOCK"
  });
}

}  // namespace

// ---------------------------------------------------------------------------
// ShortsReelsBlockerThrottle
// ---------------------------------------------------------------------------

// static
std::unique_ptr<ShortsReelsBlockerThrottle>
ShortsReelsBlockerThrottle::CreateForNavigation(
    content::NavigationThrottleRegistry& registry) {
  return base::WrapUnique(new ShortsReelsBlockerThrottle(registry));
}

ShortsReelsBlockerThrottle::ShortsReelsBlockerThrottle(
    content::NavigationThrottleRegistry& registry)
    : content::NavigationThrottle(registry) {}

ShortsReelsBlockerThrottle::~ShortsReelsBlockerThrottle() = default;

content::NavigationThrottle::ThrottleCheckResult
ShortsReelsBlockerThrottle::WillStartRequest() {
  const GURL& url = navigation_handle()->GetURL();
  if (CheckURL(url).action() == BLOCK_REQUEST) {
    return BlockRequestWithPage(url);
  }
  return PROCEED;
}

content::NavigationThrottle::ThrottleCheckResult
ShortsReelsBlockerThrottle::WillRedirectRequest() {
  const GURL& url = navigation_handle()->GetURL();
  if (CheckURL(url).action() == BLOCK_REQUEST) {
    return BlockRequestWithPage(url);
  }
  return PROCEED;
}

const char* ShortsReelsBlockerThrottle::GetNameForLogging() {
  return "ShortsReelsBlockerThrottle";
}

// static
content::NavigationThrottle::ThrottleCheckResult
ShortsReelsBlockerThrottle::BlockRequestWithPage(const GURL& url) {
  g_block_count.fetch_add(1, std::memory_order_relaxed);
  std::string html = BuildBlockPageHTML(g_block_count.load(std::memory_order_relaxed));
  return ThrottleCheckResult(BLOCK_REQUEST, net::ERR_BLOCKED_BY_CLIENT,
                             std::make_optional(std::move(html)));
}

// static
content::NavigationThrottle::ThrottleCheckResult
ShortsReelsBlockerThrottle::CheckURL(const GURL& url) {
  if (!url.is_valid() || !url.SchemeIsHTTPOrHTTPS())
    return PROCEED;

  const std::string lower_path = base::ToLowerASCII(url.path());

  // Prefix rules — O(n) but n is tiny and all comparisons are in L1 cache.
  for (const auto& rule : kPrefixBlockRules) {
    if (!url.DomainIs(rule.registrable_domain))
      continue;
    if (rule.path_prefix.empty() ||
        PathMatchesPrefix(lower_path, rule.path_prefix)) {
      return BLOCK_REQUEST;
    }
  }

  // Regex rules — only evaluated when domain matches, so rarely hit.
  for (const auto& rule : kRegexBlockRules) {
    if (url.DomainIs(rule.registrable_domain) &&
        MatchesRegexRule(rule, lower_path)) {
      return BLOCK_REQUEST;
    }
  }

  return PROCEED;
}

// static
bool ShortsReelsBlockerThrottle::PathMatchesPrefix(std::string_view path,
                                                    std::string_view prefix) {
  if (!base::StartsWith(path, prefix, base::CompareCase::SENSITIVE))
    return false;
  // Enforce component-boundary: "/shortsfilm" must NOT match "/shorts".
  return path.size() == prefix.size() || path[prefix.size()] == '/';
}

// static
void ShortsReelsBlockerThrottle::NavigateToBlockPage(
    content::WebContents* web_contents) {
  g_block_count.fetch_add(1, std::memory_order_relaxed);
  content::NavigationController::LoadURLParams params(
      GURL("chrome://distraction-blocked"));
  params.transition_type = ui::PAGE_TRANSITION_AUTO_TOPLEVEL;
  // For SPA navigations (pushState), the blocked URL is already in history.
  // Replace it so Back goes to the page before, not the blocked URL.
  params.should_replace_current_entry = true;
  web_contents->GetController().LoadURLWithParams(params);
}

// static
int ShortsReelsBlockerThrottle::GetBlockCount() {
  return g_block_count.load(std::memory_order_relaxed);
}

// ---------------------------------------------------------------------------
// ShortsReelsBlockerTabHelper
// ---------------------------------------------------------------------------

WEB_CONTENTS_USER_DATA_KEY_IMPL(ShortsReelsBlockerTabHelper);

ShortsReelsBlockerTabHelper::ShortsReelsBlockerTabHelper(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      content::WebContentsUserData<ShortsReelsBlockerTabHelper>(*web_contents) {
}

ShortsReelsBlockerTabHelper::~ShortsReelsBlockerTabHelper() = default;

void ShortsReelsBlockerTabHelper::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  // Only inspect committed main-frame navigations.
  // Same-document navigations (pushState/replaceState) satisfy both
  // conditions and are the primary target of this class.
  if (!navigation_handle->IsInPrimaryMainFrame() ||
      !navigation_handle->HasCommitted()) {
    return;
  }

  MaybeBlockURL(navigation_handle->GetURL());
}

void ShortsReelsBlockerTabHelper::MaybeBlockURL(const GURL& url) {
  // Reuse the throttle's CheckURL so block logic lives in exactly one place.
  if (ShortsReelsBlockerThrottle::CheckURL(url).action() !=
      content::NavigationThrottle::BLOCK_REQUEST) {
    return;
  }

  ShortsReelsBlockerThrottle::NavigateToBlockPage(web_contents());
}
