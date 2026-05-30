// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/content_injection/platforms/facebook_rules.h"

namespace content_injection {

namespace {

// facebook.com — hide posts, stories shelf, and loading skeletons on the home
// feed. Neutralizes IntersectionObserver-based infinite scroll triggers and
// locks body scroll on the root path.
//
// SPA-aware: patches pushState/replaceState + popstate listener.
constexpr std::string_view kFacebookJS = R"JS(
const style=document.createElement('style');
style.textContent='body{background-color:rgb(36 37 38)!important;}'+
'[data-tracking-duration-id]{display:none!important;}'+
'[data-srat="43"]{display:none!important;}.hscroller{display:none!important;}'+
'[data-on-first-inserted-action-id]{display:none!important;}';
document.head.appendChild(style);
document.querySelectorAll('[data-trigger-type="1"]').forEach(el=>{el.style.display='none'});
const obs=new IntersectionObserver(()=>{});
document.querySelectorAll('[data-marker-id]').forEach(el=>obs.observe(el));
function applyScrollLock(){
  document.documentElement.style.overflowY=(location.pathname==='/')?'hidden':'';
}
applyScrollLock();
['pushState','replaceState'].forEach(m=>{
  const orig=history[m];
  history[m]=function(...args){const r=orig.apply(this,args);applyScrollLock();return r;};
});
window.addEventListener('popstate',applyScrollLock);
)JS";

constexpr InjectionRule kRules[] = {
    {"facebook.com", "", InjectionType::kJavaScript, kFacebookJS},
};

}  // namespace

base::span<const InjectionRule> GetFacebookInjectionRules() {
  return kRules;
}

}  // namespace content_injection
