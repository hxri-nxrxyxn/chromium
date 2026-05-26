// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/distraction_blocked/distraction_blocked_ui.h"

#include <string>

#include "base/memory/ref_counted_memory.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/navigation_policy/shorts_reels_blocker.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/url_constants.h"
#include "chrome/common/webui_url_constants.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "url/gurl.h"

namespace {

// Builds the full HTML page with the current block count embedded.
std::string BuildPageHTML(int block_count) {
  std::string counter_text = base::StringPrintf(
      "blocked %d time%s this session",
      block_count, block_count == 1 ? "" : "s");

  return base::StrCat({R"HTML(
<!doctype html>
<html>
<head>
<meta charset="utf-8">
<meta name="color-scheme" content="light dark">
<meta name="theme-color" content="#fff">
<meta name="viewport" content="width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no">
<title>Page Blocked</title>
<style>
a{color:var(--link-color)}
body{
  --background-color:#fff;
  --error-code-color:var(--google-gray-700);
  --google-blue-50:rgb(232,240,254);
  --google-blue-100:rgb(210,227,252);
  --google-blue-300:rgb(138,180,248);
  --google-blue-600:rgb(26,115,232);
  --google-blue-700:rgb(25,103,210);
  --google-gray-100:rgb(241,243,244);
  --google-gray-300:rgb(218,220,224);
  --google-gray-500:rgb(154,160,166);
  --google-gray-50:rgb(248,249,250);
  --google-gray-600:rgb(128,134,139);
  --google-gray-700:rgb(95,99,104);
  --google-gray-800:rgb(60,64,67);
  --google-gray-900:rgb(32,33,36);
  --heading-color:var(--google-gray-900);
  --link-color:rgb(88,88,88);
  --text-color:var(--google-gray-700);
  background:var(--background-color);
  color:var(--text-color);
  font-family:-apple-system,BlinkMacSystemFont,"Segoe UI",Roboto,Oxygen,Ubuntu,Cantarell,"Fira Sans","Droid Sans","Helvetica Neue",sans-serif;
  word-wrap:break-word;
  margin:0;padding:0
}
html{-webkit-text-size-adjust:100%;font-size:125%}
h1{color:var(--heading-color);font-size:1.6em;font-weight:normal;line-height:1.25em;margin-bottom:16px;margin-top:0;word-wrap:break-word}
h1 span{font-weight:500}
p{color:var(--text-color);font-size:1.1em;margin-top:8px;line-height:1.55}
.icon{background-repeat:no-repeat;background-size:100%;display:inline-block;height:72px;margin:0 0 40px;width:72px;-webkit-user-select:none}
.icon-blocked{background-image:url("data:image/svg+xml,%3Csvg xmlns='http://www.w3.org/2000/svg' width='72' height='72' viewBox='0 0 72 72'%3E%3Ccircle cx='36' cy='36' r='32' fill='%23ea4335'/%3E%3Crect x='22' y='32' width='28' height='8' rx='4' fill='%23fff'/%3E%3C/svg%3E")}
.error-code{color:var(--error-code-color);font-size:.8em;margin-top:12px;text-transform:lowercase}
.nav-wrapper{margin-top:51px}
.nav-wrapper::after{clear:both;content:'';display:table;width:100%}
.secondary-button{background:var(--secondary-button-fill-color,#fff);border:1px solid var(--secondary-button-border-color,var(--google-gray-500));border-radius:20px;box-sizing:border-box;color:var(--secondary-button-text-color,var(--google-gray-700));cursor:pointer;display:inline-block;font-size:.875em;padding:8px 16px;text-decoration:none;user-select:none}
.secondary-button:hover{background:var(--secondary-button-hover-fill-color,var(--google-gray-50));border-color:var(--secondary-button-hover-border-color,var(--google-gray-600))}
.interstitial-wrapper{box-sizing:border-box;font-size:1em;line-height:1.6em;margin:14vh auto 0;max-width:600px;width:100%;padding:0 24px}
#main-content{padding-bottom:40px}
@media(prefers-color-scheme:dark){
  body{
    --background-color:var(--google-gray-900);
    --error-code-color:var(--google-gray-500);
    --heading-color:var(--google-gray-500);
    --link-color:var(--google-blue-300);
    --text-color:var(--google-gray-500)
  }
  .icon-blocked{background-image:url("data:image/svg+xml,%3Csvg xmlns='http://www.w3.org/2000/svg' width='72' height='72' viewBox='0 0 72 72'%3E%3Ccircle cx='36' cy='36' r='32' fill='%23f28b82'/%3E%3Crect x='22' y='32' width='28' height='8' rx='4' fill='%23202124'/%3E%3C/svg%3E")}
  .secondary-button{background:var(--google-gray-900);border-color:var(--google-gray-700);color:var(--google-blue-300)}
  .secondary-button:hover{background:rgb(48,51,57)}
}
@media(max-width:700px){.interstitial-wrapper{padding:0 10%}}
@media(max-width:420px){
  .interstitial-wrapper{padding:0 5%}
  h1{font-size:1.5em;margin-bottom:8px}
  .icon{margin-bottom:5.69vh}
  .interstitial-wrapper{margin:7vh auto 12px;padding:0 24px}
}
</style>
</head>
<body>
<div id="content">
  <div id="main-frame-error" class="interstitial-wrapper">
    <div id="main-content">
      <div class="icon icon-blocked"></div>
      <div id="main-message">
        <h1><span>This page was blocked</span></h1>
        <p>Short-form videos aren't available here. That was your call.</p>
        <div class="error-code">)HTML",
      counter_text,
      R"HTML(</div>
      </div>
      <div class="nav-wrapper">
        <button class="secondary-button" onclick="window.history.back()">Back to previous page</button>
      </div>
    </div>
  </div>
</div>
</body>
</html>)HTML"});
}

void CreateAndAddDistractionBlockedHTMLSource(Profile* profile) {
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      profile, chrome::kChromeUIDistractionBlockedHost);

  source->SetRequestFilter(
      base::BindRepeating([](const std::string& path) {
        return path.empty() || path == "index.html";
      }),
      base::BindRepeating(
          [](const std::string& path,
             content::WebUIDataSource::GotDataCallback callback) {
            int count = ShortsReelsBlockerThrottle::GetBlockCount();
            std::string html = BuildPageHTML(count);
            std::move(callback).Run(
                base::MakeRefCounted<base::RefCountedString>(
                    std::move(html)));
          }));
}

}  // namespace

DistractionBlockedUI::DistractionBlockedUI(content::WebUI* web_ui)
    : content::WebUIController(web_ui) {
  CreateAndAddDistractionBlockedHTMLSource(Profile::FromWebUI(web_ui));
}

DistractionBlockedUI::~DistractionBlockedUI() = default;
