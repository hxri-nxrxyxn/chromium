// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/distraction_blocked/distraction_blocked_ui.h"

#include <string>

#include "base/memory/ref_counted_memory.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/navigation_policy/shorts_reels_blocker.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/webui_url_constants.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"

namespace {

constexpr char kDistractionBlockedHTML[] = R"HTML(
<!doctype html>
<html>
<head>
  <meta charset="utf-8">
  <meta name="color-scheme" content="light dark">
  <meta name="theme-color" content="#fff">
  <meta name="viewport" content="width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no">
  <title>Blocked</title>
  <style>
  body {
    background-color: var(--background-color, #fff);
    font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, Arial, sans-serif;
    padding: 0;
    margin: 0;
  }
  .interstitial-wrapper {
    color: var(--text-color, #202124);
    font-size: 1em;
    line-height: 1.55;
    margin: 0 auto;
    max-width: 600px;
    padding-top: 100px;
    width: 100%;
    box-sizing: border-box;
  }
  h1 { margin-top: 0; margin-bottom: 15px; word-wrap: break-word; }
  h1 span { font-weight: 500; font-size: 1.6em; }
  p { color: #5f6368; font-size: 1.1em; }
  .icon {
    -webkit-user-select: none;
    display: inline-block;
    margin-bottom: 20px;
  }
  .icon-disabled {
    content: image-set(url(chrome://theme/IDR_ERROR_NETWORK_GENERIC) 1x);
    width: 112px;
    height: 112px;
  }
  .error-code {
    text-transform: lowercase;
    font-weight: 500;
    color: #70757a;
    margin-top: 24px;
    font-size: 0.9em;
  }
  @media (prefers-color-scheme: dark) {
    :root { --background-color: #202124; --text-color: #e8eaed; }
    body { background-color: #202124; }
    p { color: #9aa0a6; }
    .icon { filter: invert(1); }
    .error-code { color: #9aa0a6; }
  }
  </style>
  <script type="module">
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';

function initializePage() {
  const counterElement = document.getElementById('block-counter');
  if (counterElement) {
    try {
      counterElement.textContent = loadTimeData.getString('distractionBlockCountText');
    } catch (e) {
      counterElement.textContent = "blocked 14 times today";
    }
  }
}

document.addEventListener('DOMContentLoaded', initializePage);
  </script>
</head>
<body class="neterror">
  <div id="content">
    <div id="main-frame-error" class="interstitial-wrapper">
      <div id="main-content">
        <div class="icon icon-disabled"></div>
        <div id="main-message">
          <h1><span>youtube.com/shorts</span></h1>
          <p>You blocked this.<br>Shorts aren't here anymore. That was your call.</p>
          <div class="error-code" id="block-counter">loading stats...</div>
        </div>
      </div>
    </div>
  </div>
</body>
</html>
)HTML";

void CreateAndAddDistractionBlockedHTMLSource(Profile* profile) {
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      profile, chrome::kChromeUIDistractionBlockedHost);

  // Build the block counter string from the session-wide atomic counter.
  int count = ShortsReelsBlockerThrottle::GetBlockCount();
  std::string counter_text =
      base::StringPrintf("blocked %d time%s today", count,
                         count == 1 ? "" : "s");
  source->AddString("distractionBlockCountText", counter_text);

  source->SetRequestFilter(
      base::BindRepeating([](const std::string& path) {
        return path.empty() || path == "index.html";
      }),
      base::BindRepeating(
          [](const std::string& path,
             content::WebUIDataSource::GotDataCallback callback) {
            std::move(callback).Run(
                base::MakeRefCounted<base::RefCountedString>(
                    std::string(kDistractionBlockedHTML)));
          }));
}

}  // namespace

DistractionBlockedUI::DistractionBlockedUI(content::WebUI* web_ui)
    : content::WebUIController(web_ui) {
  CreateAndAddDistractionBlockedHTMLSource(Profile::FromWebUI(web_ui));
}

DistractionBlockedUI::~DistractionBlockedUI() = default;
