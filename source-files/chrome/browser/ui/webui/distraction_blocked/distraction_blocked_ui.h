// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_DISTRACTION_BLOCKED_DISTRACTION_BLOCKED_UI_H_
#define CHROME_BROWSER_UI_WEBUI_DISTRACTION_BLOCKED_DISTRACTION_BLOCKED_UI_H_

#include "chrome/common/webui_url_constants.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/webui_config.h"

class DistractionBlockedUI;

class DistractionBlockedUIConfig
    : public content::DefaultWebUIConfig<DistractionBlockedUI> {
 public:
  DistractionBlockedUIConfig()
      : DefaultWebUIConfig(content::kChromeUIScheme,
                           chrome::kChromeUIDistractionBlockedHost) {}
};

class DistractionBlockedUI : public content::WebUIController {
 public:
  explicit DistractionBlockedUI(content::WebUI* web_ui);
  ~DistractionBlockedUI() override;

  DistractionBlockedUI(const DistractionBlockedUI&) = delete;
  DistractionBlockedUI& operator=(const DistractionBlockedUI&) = delete;
};

#endif  // CHROME_BROWSER_UI_WEBUI_DISTRACTION_BLOCKED_DISTRACTION_BLOCKED_UI_H_
