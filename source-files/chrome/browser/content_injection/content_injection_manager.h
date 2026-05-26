// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTENT_INJECTION_CONTENT_INJECTION_MANAGER_H_
#define CHROME_BROWSER_CONTENT_INJECTION_CONTENT_INJECTION_MANAGER_H_

#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

class GURL;

namespace content {
class RenderFrameHost;
}

namespace content_injection {

// Per-tab manager that fires CSS/JS injections from the rule table
// (content_injection_rules.h) at DOMContentLoaded time.
//
// Injections run in an isolated world (ISOLATED_WORLD_ID_CONTENT_END) so:
//   - The page's own JS cannot observe or tamper with injected code.
//   - Injected code cannot accidentally pollute the page's JS globals.
//   - The page can still read injected DOM changes (style, removed elements).
//
// Only the primary main frame receives injections; subframes and fenced
// frames are intentionally excluded.
class ContentInjectionManager final
    : public content::WebContentsObserver,
      public content::WebContentsUserData<ContentInjectionManager> {
 public:
  ~ContentInjectionManager() override;

  ContentInjectionManager(const ContentInjectionManager&) = delete;
  ContentInjectionManager& operator=(const ContentInjectionManager&) = delete;

 private:
  explicit ContentInjectionManager(content::WebContents* web_contents);
  friend class content::WebContentsUserData<ContentInjectionManager>;

  // content::WebContentsObserver:
  // DOMContentLoaded fires after the DOM is parsed but before subresources
  // finish loading — early enough to avoid a flash of unwanted content,
  // late enough that document.head exists for style injection.
  void DOMContentLoaded(
      content::RenderFrameHost* render_frame_host) override;

  // Iterates the rule table and fires every rule matching |url| in |frame|.
  void RunMatchingRules(content::RenderFrameHost* frame, const GURL& url);

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace content_injection

#endif  // CHROME_BROWSER_CONTENT_INJECTION_CONTENT_INJECTION_MANAGER_H_
