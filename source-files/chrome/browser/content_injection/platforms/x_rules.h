// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTENT_INJECTION_PLATFORMS_X_RULES_H_
#define CHROME_BROWSER_CONTENT_INJECTION_PLATFORMS_X_RULES_H_

#include "base/containers/span.h"
#include "chrome/browser/content_injection/content_injection_rules.h"

namespace content_injection {
base::span<const InjectionRule> GetXInjectionRules();
}  // namespace content_injection

#endif  // CHROME_BROWSER_CONTENT_INJECTION_PLATFORMS_X_RULES_H_
