// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NAVIGATION_POLICY_PLATFORM_RULES_REDDIT_BLOCK_RULES_H_
#define CHROME_BROWSER_NAVIGATION_POLICY_PLATFORM_RULES_REDDIT_BLOCK_RULES_H_

#include "base/containers/span.h"
#include "chrome/browser/navigation_policy/platform_rules/block_rule_types.h"

base::span<const BlockRule> GetRedditPrefixRules();
// Reddit mobile share short-links require a regex rule.
base::span<const RegexBlockRule> GetRedditRegexRules();

#endif  // CHROME_BROWSER_NAVIGATION_POLICY_PLATFORM_RULES_REDDIT_BLOCK_RULES_H_
