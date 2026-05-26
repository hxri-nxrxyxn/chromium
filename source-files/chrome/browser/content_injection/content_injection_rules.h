// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTENT_INJECTION_CONTENT_INJECTION_RULES_H_
#define CHROME_BROWSER_CONTENT_INJECTION_CONTENT_INJECTION_RULES_H_

#include <string_view>

#include "base/containers/span.h"

namespace content_injection {

enum class InjectionType {
  kCSS,
  kJavaScript,
};

struct InjectionRule {
  // Matched via GURL::DomainIs() — covers subdomains automatically.
  std::string_view registrable_domain;

  // Lower-cased path prefix. Empty string matches all paths on the domain.
  // Unlike the blocker, prefix matching here is NOT component-boundary
  // enforced so that e.g. "/feed" matches "/feed/", "/feed/following", etc.
  std::string_view path_prefix;

  InjectionType type;

  // Raw CSS or JS to inject. CSS is wrapped in a <style> element at
  // injection time; JS is executed as-is in an isolated world.
  std::string_view payload;
};

// Returns the full ordered rule table. All matching rules for a given URL
// are applied — later rules do not cancel earlier ones.
base::span<const InjectionRule> GetInjectionRules();

}  // namespace content_injection

#endif  // CHROME_BROWSER_CONTENT_INJECTION_CONTENT_INJECTION_RULES_H_
