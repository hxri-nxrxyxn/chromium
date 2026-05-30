(() => {
  // ── Remove any leftover style injected by previous script runs ──
  var old = document.getElementById('reddit-style');
  if (old) old.parentNode.removeChild(old);

  // ── Inject styles to hide Reddit Home Feed elements ──
  const style = document.createElement('style');
  style.id = 'reddit-style';
  style.textContent = `
    /* Hide the home feed element */
    shreddit-feed,
    /* Hide feed page loading animations/spinners */
    shreddit-feed-page-loading,
    /* Hide early stage faceplate loaders for HomeFeed */
    faceplate-loader[name*="HomeFeed"],
    /* Hide Lit-based suspense placeholders for the HomeFeed */
    suspense-placeholder[name="HomeFeed"] {
      display: none !important;
    }
  `;
  document.head.appendChild(style);

  // ── Lock scroll on home route ("/") only ──
  function applyScrollLock() {
    if (location.pathname === '/') {
      document.documentElement.style.overflowY = 'hidden';
    } else {
      document.documentElement.style.overflowY = '';
    }
  }

  // Run on load
  applyScrollLock();

  // Patch pushState/replaceState to detect SPA navigation
  ['pushState', 'replaceState'].forEach(method => {
    const original = history[method];
    history[method] = function (...args) {
      const result = original.apply(this, args);
      applyScrollLock();
      return result;
    };
  });

  // Also handle back/forward navigation
  window.addEventListener('popstate', applyScrollLock);

  console.log("Reddit distractions hidden! Home feed blocked, search/threads active.");
})();
