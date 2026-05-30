(function() {
  // Helper to check route
  function applyBlock() {
    let path = window.location.pathname;

    // Support local file testing of the saved HTML snapshots
    if (path.endsWith('/Instagram.html') || path.endsWith('/instagram.html')) {
      path = '/';
    } else if (path.endsWith('/explore.html')) {
      path = '/explore';
    }

    if (path === '/' || path === '') {
      cleanupExploreBlock();
      applyHomeBlock();
    } else if (path.startsWith('/explore')) {
      cleanupHomeBlock();
      applyExploreBlock();
    } else {
      cleanupExploreBlock();
      cleanupHomeBlock();
    }
  }

  function applyHomeBlock() {
    // 1. Hide Stories Tray
    const storyTray = document.querySelector('[data-pagelet="story_tray"]') || document.querySelector('a[href^="/stories/"]')?.closest('div');
    if (storyTray && !storyTray.hidden) {
      storyTray.setAttribute('data-org-class', storyTray.className);
      storyTray.className = '';
      storyTray.hidden = true;
    }

    // 2. Hide Main Feed Column (the main element)
    const mainEl = document.querySelector('main') || document.querySelector('div[role="main"]');
    if (mainEl && !mainEl.hasAttribute('data-focus-blocked-home')) {
      mainEl.setAttribute('data-focus-blocked-home', 'true');
      mainEl.setAttribute('data-org-class-main', mainEl.className);
      mainEl.className = '';
      mainEl.hidden = true;
      
      // Unload all video assets inside mainEl to save bandwidth/CPU
      mainEl.querySelectorAll('video').forEach(video => {
        try {
          video.pause();
          video.removeAttribute('src');
          video.querySelectorAll('source').forEach(src => src.removeAttribute('src'));
          video.load();
        } catch (e) {}
      });
    }

    // Hide any loading spinners
    document.querySelectorAll('svg[aria-label="Loading..."], [role="progressbar"], div[aria-label="Loading..."], [data-visualcompletion="loading-state"]').forEach(spinner => {
      spinner.hidden = true;
    });
  }

  function cleanupHomeBlock() {
    // 1. Restore Stories Tray
    const storyTray = document.querySelector('[data-pagelet="story_tray"]') || document.querySelector('a[href^="/stories/"]')?.closest('div');
    if (storyTray && storyTray.hidden) {
      if (storyTray.hasAttribute('data-org-class')) {
        storyTray.className = storyTray.getAttribute('data-org-class');
        storyTray.removeAttribute('data-org-class');
      }
      storyTray.hidden = false;
    }

    // 2. Restore Main element
    const mainEl = document.querySelector('main') || document.querySelector('div[role="main"]');
    if (mainEl && mainEl.hasAttribute('data-focus-blocked-home')) {
      mainEl.removeAttribute('data-focus-blocked-home');
      if (mainEl.hasAttribute('data-org-class-main')) {
        mainEl.className = mainEl.getAttribute('data-org-class-main');
        mainEl.removeAttribute('data-org-class-main');
      }
      mainEl.hidden = false;
    }
  }

  function applyExploreBlock() {
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
        if (!child.hasAttribute('data-org-class')) {
          child.setAttribute('data-org-class', child.className);
        }
        child.hidden = true;
        child.className = '';
      });
    }

    // Hide any loading spinners on the Explore page
    document.querySelectorAll('svg[aria-label="Loading..."], [role="progressbar"], div[aria-label="Loading..."], [data-visualcompletion="loading-state"]').forEach(spinner => {
      spinner.hidden = true;
    });
  }

  function cleanupExploreBlock() {
    const mainEl = document.querySelector('main') || document.querySelector('div[role="main"]');
    if (mainEl && mainEl.hasAttribute('data-focus-blocked')) {
      mainEl.removeAttribute('data-focus-blocked');
      
      // Restore all hidden children elements of mainEl
      Array.from(mainEl.children).forEach(child => {
        if (child.hasAttribute('data-org-class')) {
          child.className = child.getAttribute('data-org-class');
          child.removeAttribute('data-org-class');
        }
        child.hidden = false;
      });
    }
  }

  // Initial sweep
  applyBlock();

  // Watch for dynamic infinite scroll injections and page mutations
  const observer = new MutationObserver(() => {
    observer.disconnect();
    try {
      applyBlock();
    } catch (e) {}
    observer.observe(document.body, { childList: true, subtree: true });
  });

  observer.observe(document.body, { childList: true, subtree: true });

  // Patch pushState/replaceState to detect SPA navigation immediately
  ['pushState', 'replaceState'].forEach(method => {
    const original = history[method];
    history[method] = function (...args) {
      const result = original.apply(this, args);
      setTimeout(applyBlock, 0);
      return result;
    };
  });

  // Also handle browser history back/forward popstate event
  window.addEventListener('popstate', () => {
    setTimeout(applyBlock, 0);
  });

  console.log("Instagram Consolidated Blocker Active (Home and Explore page blockers merged successfully)");
})();
