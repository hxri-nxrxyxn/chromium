// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NAVIGATION_POLICY_PLATFORM_RULES_ALL_BLOCK_RULES_H_
#define CHROME_BROWSER_NAVIGATION_POLICY_PLATFORM_RULES_ALL_BLOCK_RULES_H_

#include <vector>

#include "chrome/browser/navigation_policy/platform_rules/block_rule_types.h"

// Returns the union of all platform prefix block rules, lazily initialized
// on first call and cached for the process lifetime.
const std::vector<BlockRule>& GetAllPrefixBlockRules();

// Returns the union of all platform regex block rules, lazily initialized
// on first call and cached for the process lifetime.
const std::vector<RegexBlockRule>& GetAllRegexBlockRules();

#endif  // CHROME_BROWSER_NAVIGATION_POLICY_PLATFORM_RULES_ALL_BLOCK_RULES_H_
