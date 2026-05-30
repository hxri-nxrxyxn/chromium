(() => {
  // ── Remove any leftover style injected by previous script runs ──
  var old = document.getElementById('pdb-style');
  if (old) old.parentNode.removeChild(old);

  // ── Restore nav/header visibility in case a previous run hid them ──
  var nav = document.getElementById('VerticalNavContent');
  if (nav) nav.style.removeProperty('display');
  var header = document.getElementById('HeaderContent');
  if (header) header.style.removeProperty('display');
  // ── Selectors for distraction content to hide ──
  var SELECTORS = [
    '[data-test-id="default-tab"]',
    '[data-test-id="search-story-suggestions-container"]',
    '[data-test-id="search-suggestion-curated-board-bubble"]',
    '[data-test-id="carousel-bubble-wrapper-slp_immersive_header"]',
    '[data-test-id="homefeed-feed"]',
    '[data-test-id="masonry-container"]',
    '[data-test-id="max-width-container"]',
    '[data-test-id="closeup-feed"]',
    '[data-test-id="related-pins-grid"]',
    '[data-test-id="floating-footer"]',
    '[data-test-id="more-ideas-tabs"]',
    '[data-root-margin="more-ideas-tabs"]',
  ];

  function hideAll() {
    // Remove the feed tablist container completely (but NOT the main navigation tablist)
    const tablist = document.querySelector('[data-root-margin="more-ideas-tabs"] [role="tablist"]') || 
                    document.querySelector('[data-test-id="homefeed-feed"] [role="tablist"]') ||
                    document.querySelector('[role="tablist"]:has(#homefeed)');
    if (tablist) {
      tablist.remove();
    }

    // Remove the funny school quotes page container block
    const quotesBanner = document.querySelector('a[href*="/school-quotes-funny/"]');
    if (quotesBanner) {
      quotesBanner.remove();
    }

    SELECTORS.forEach(function (sel) {
      document.querySelectorAll(sel).forEach(function (el) {
        el.style.setProperty('display', 'none', 'important');
      });
    });
    // Always keep nav and header visible
    var n = document.getElementById('VerticalNavContent');
    if (n) n.style.removeProperty('display');
    var h = document.getElementById('HeaderContent');
    if (h) h.style.removeProperty('display');
  }

  hideAll();

  // ── Observe DOM mutations so dynamically injected elements are hidden too ──
  var observer = new MutationObserver(hideAll);
  observer.observe(document.body, { childList: true, subtree: true });

  // ── Re-apply on SPA navigation ──
  ['pushState', 'replaceState'].forEach(function (m) {
    var orig = history[m];
    history[m] = function () {
      var r = orig.apply(this, arguments);
      setTimeout(hideAll, 100);
      setTimeout(hideAll, 500);
      return r;
    };
  });
  window.addEventListener('popstate', function () {
    setTimeout(hideAll, 100);
  });

  console.log('Done - feed hidden, tablist/quotes removed, navbar and header preserved.');
})();
