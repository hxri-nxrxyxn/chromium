// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/content_injection/content_injection_manager.h"

#include <string>

#include "base/json/json_writer.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/content_injection/content_injection_rules.h"
#include "content/public/common/isolated_world_ids.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "url/gurl.h"

namespace content_injection {

WEB_CONTENTS_USER_DATA_KEY_IMPL(ContentInjectionManager);

ContentInjectionManager::ContentInjectionManager(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      content::WebContentsUserData<ContentInjectionManager>(*web_contents) {}

ContentInjectionManager::~ContentInjectionManager() = default;

void ContentInjectionManager::DOMContentLoaded(
    content::RenderFrameHost* render_frame_host) {
  // Only inject into the primary main frame.
  if (!render_frame_host->IsInPrimaryMainFrame())
    return;

  RunMatchingRules(render_frame_host,
                   render_frame_host->GetLastCommittedURL());
}

void ContentInjectionManager::RunMatchingRules(
    content::RenderFrameHost* frame,
    const GURL& url) {
  if (!url.is_valid() || !url.SchemeIsHTTPOrHTTPS())
    return;

  const std::string lower_path = base::ToLowerASCII(url.path());

  for (const InjectionRule& rule : GetInjectionRules()) {
    if (!url.DomainIs(rule.registrable_domain))
      continue;

    // Empty path_prefix matches any path on the domain.
    // Non-empty prefix: simple StartsWith (intentionally not component-boundary
    // enforced so "/feed" matches "/feed/following" etc.).
    if (!rule.path_prefix.empty() &&
        !base::StartsWith(lower_path, rule.path_prefix,
                          base::CompareCase::SENSITIVE)) {
      continue;
    }

    std::string script;

    if (rule.type == InjectionType::kCSS) {
      // Wrap CSS in a <style> element via JS so we have one uniform injection
      // path. The CSS string is JSON-encoded to safely escape quotes, newlines,
      // and any other characters that would break the JS string literal.
      std::string json_css;
      base::JSONWriter::Write(base::Value(std::string(rule.payload)), &json_css);
      script =
          "(function(){"
          "var s=document.createElement('style');"
          "s.textContent=" + json_css + ";"
          "document.head.appendChild(s);"
          "})();";
    } else {
      // kJavaScript: inject as-is; payloads are already IIFEs.
      script = std::string(rule.payload);
    }

    frame->ExecuteJavaScriptInIsolatedWorld(
        base::UTF8ToUTF16(script),
        /*callback=*/base::NullCallback(),
        content::ISOLATED_WORLD_ID_CONTENT_END);
  }
}

}  // namespace content_injection
