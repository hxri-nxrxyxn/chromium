(function() {
  const style = document.createElement('style');
  style.textContent = `
    /* --- Hide Home Feed & Timeline Navigation Tabs --- */
    /* Hides the left navigation banner entirely */
    header[role="banner"],
    /* Hides the home timeline feed list specifically */
    [aria-label="Timeline: Your Home Timeline"],
    [aria-label="Timeline: Home timeline"],
    /* Hides the explore page trending/news feed list specifically */
    [aria-label="Timeline: Explore"],
    /* Hides the "For You" / "Following" tab bar list at the top */
    [role="tablist"],
    [data-testid="ScrollSnap-List"] {
      display: none !important;
    }

    /* --- Hide Right Sidebar Distractions (Trends, Who to Follow) --- */
    /* Hides What's Happening and Who to Follow modules (keeps search bar) */
    [data-testid="sidebarColumn"] section,
    [aria-label="Who to follow"],
    [aria-label="Relevant people"],
    [aria-label="Trending"],
    [aria-label="Timeline: Trending now"] {
      display: none !important;
    }

    /* --- Hide Sidebar Navigation Items --- */
    /* Grok Link */
    a[href="/i/grok"],
    [data-testid="AppTabBar_Grok_Link"],
    /* Jobs Link */
    a[href="/jobs"],
    /* Premium Upsell */
    a[href="/i/premium_sign_up"],
    [aria-label="Premium"] {
      display: none !important;
    }
  `;
  document.head.appendChild(style);
  console.log("X (Twitter) distractions hidden! Home feed blocked, sidebar cleaned, search/compose/threads active.");
})();
