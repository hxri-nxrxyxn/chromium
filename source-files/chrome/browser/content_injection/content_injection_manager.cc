// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/content_injection/content_injection_manager.h"

#include <string>

#include "base/base64.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/content_injection/content_injection_rules.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/isolated_world_ids.h"
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

    std::u16string script;

    if (rule.type == InjectionType::kCSS) {
      // Encode the CSS as Base64 and construct a data-URI stylesheet link.
      // This is the safest injection technique:
      //   - No string escaping needed — Base64 is safe in any JS context.
      //   - No risk of CSS content breaking out of a JS string literal.
      //   - The <link> element approach keeps styles in the document's
      //     own cascade, visible to DevTools, and avoids innerHTML parsing.
      //   - Runs in an isolated world so page JS cannot observe or tamper
      //     with the injected element via document.stylesheets.
      const std::string encoded =
          base::Base64Encode(std::string_view(rule.payload));
      script = base::UTF8ToUTF16(
          "(function(){"
          "var l=document.createElement('link');"
          "l.rel='stylesheet';"
          "l.href='data:text/css;base64," + encoded + "';"
          "document.head.appendChild(l);"
          "})();");
    } else {
      // kJavaScript: inject as-is in an isolated world; payloads are IIFEs.
      script = base::UTF8ToUTF16(std::string(rule.payload));
    }

    frame->ExecuteJavaScriptInIsolatedWorld(
        script,
        /*callback=*/base::NullCallback(),
        content::ISOLATED_WORLD_ID_CONTENT_END);
  }
}

}  // namespace content_injection
