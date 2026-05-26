// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/navigation_policy/shorts_reels_blocker.h"

#include <atomic>
#include <string_view>

#include "base/memory/ptr_util.h"

#include "base/strings/string_util.h"
#include "chrome/common/webui_url_constants.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/navigation_throttle_registry.h"
#include "content/public/browser/web_contents.h"
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

}  // namespace

// ---------------------------------------------------------------------------
// Session-wide block counter (atomic, not persisted across restarts).
// ---------------------------------------------------------------------------
namespace {
std::atomic<int> g_block_count{0};
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
  if (CheckURL(navigation_handle()->GetURL()).action() == BLOCK_REQUEST) {
    NavigateToBlockPage(navigation_handle()->GetWebContents());
    return CANCEL_AND_IGNORE;
  }
  return PROCEED;
}

content::NavigationThrottle::ThrottleCheckResult
ShortsReelsBlockerThrottle::WillRedirectRequest() {
  if (CheckURL(navigation_handle()->GetURL()).action() == BLOCK_REQUEST) {
    NavigateToBlockPage(navigation_handle()->GetWebContents());
    return CANCEL_AND_IGNORE;
  }
  return PROCEED;
}

const char* ShortsReelsBlockerThrottle::GetNameForLogging() {
  return "ShortsReelsBlockerThrottle";
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
