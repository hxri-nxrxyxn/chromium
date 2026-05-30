(function() {
  function applyBlock() {
    if (window.location.pathname.startsWith('/explore')) {
      const mainEl = document.querySelector('main') || document.querySelector('div[role="main"]');
      if (mainEl && !mainEl.hasAttribute('data-focus-blocked')) {
        mainEl.setAttribute('data-focus-blocked', 'true');
        
        // 1. Unload all video assets inside the blocked element
        mainEl.querySelectorAll('video').forEach(video => {
          try {
            video.pause();
            video.removeAttribute('src');
            video.querySelectorAll('source').forEach(src => src.removeAttribute('src'));
            video.load();
          } catch (e) {}
        });

        // 2. Hide all original children elements (such as the explore grid and rows)
        // but preserve any child containing search inputs or headers
        Array.from(mainEl.children).forEach(child => {
          if (child.querySelector('input') || child.querySelector('[role="search"]') || child.tagName === 'HEADER') {
            return;
          }
          child.hidden = true;
          child.className = '';
        });
      }
    }

    // Hide any loading spinners on the Explore page
    document.querySelectorAll('svg[aria-label="Loading..."], [role="progressbar"]').forEach(spinner => {
      spinner.hidden = true;
    });
  }

  // Initial sweep
  applyBlock();

  // Watch for dynamic Explore page mounting/updates
  const observer = new MutationObserver(() => {
    observer.disconnect();
    try {
      applyBlock();
    } catch (e) {}
    observer.observe(document.body, { childList: true, subtree: true });
  });

  observer.observe(document.body, { childList: true, subtree: true });

  console.log("Instagram Explore Blocker Active (Entire explore grid hidden, search bar kept)");
})();
