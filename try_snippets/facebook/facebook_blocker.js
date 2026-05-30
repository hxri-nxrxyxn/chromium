(function() {
  const suggestionKeywords = [
    'suggested for you',
    'suggested post',
    'suggested groups',
    'suggested pages',
    'suggested events',
    'people you may know',
    'popular now'
  ];

  // Helper to walk up the DOM and find the outermost container of the stories tray
  function findStoriesContainer(link) {
    let curr = link;
    while (curr && curr.parentElement && curr.parentElement !== document.body) {
      const parent = curr.parentElement;
      if (parent.tagName === 'UL' || parent.getAttribute('role') === 'region' || parent.getAttribute('aria-label') === 'Stories' || parent.getAttribute('data-testid') === 'stories_tray') {
        return parent;
      }
      if (parent.querySelectorAll('a[href*="/stories/"]').length > 1) {
        return parent.parentElement || parent;
      }
      curr = parent;
    }
    return null;
  }

  // Helper to hide elements and replace them with a canvas placeholder
  function hideAndUnload(element, messageText) {
    if (element && !element.hasAttribute('data-focus-blocked')) {
      element.setAttribute('data-focus-blocked', 'true');
      
      // 1. Unload all video assets inside the blocked element
      element.querySelectorAll('video').forEach(video => {
        try {
          video.pause();
          video.removeAttribute('src');
          video.querySelectorAll('source').forEach(src => src.removeAttribute('src'));
          video.load();
        } catch (e) {}
      });

      // 2. Measure original size
      const rect = element.getBoundingClientRect();
      const width = rect.width > 0 ? rect.width : 500;
      const height = rect.height > 0 ? rect.height : 400;

      // 3. Hide all original children
      Array.from(element.children).forEach(child => {
        child.hidden = true;
        child.className = '';
      });

      // 4. Draw canvas placeholder
      const canvas = document.createElement('canvas');
      canvas.setAttribute('data-focus-placeholder', 'true');
      canvas.width = width;
      canvas.height = height;
      
      const ctx = canvas.getContext('2d');
      if (ctx) {
        ctx.fillStyle = '#ffffff';
        ctx.fillRect(0, 0, width, height);

        ctx.strokeStyle = '#dbdbdb'; // native-feeling grey
        ctx.lineWidth = 1;
        ctx.setLineDash([6, 6]);
        ctx.strokeRect(10, 10, width - 20, height - 20);

        ctx.fillStyle = '#8e8e8e'; // secondary text grey
        ctx.font = '13px -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, Helvetica, Arial, sans-serif';
        ctx.textAlign = 'center';
        ctx.textBaseline = 'middle';
        ctx.fillText(messageText, width / 2, height / 2);
      }
      
      element.appendChild(canvas);
    }
  }

  function applyBlock() {
    const path = window.location.pathname;

    // A. Block Video feed / Reels routes entirely
    if (path.startsWith('/reel') || path.startsWith('/watch') || path.startsWith('/video') || path.startsWith('/gaming')) {
      const mainEl = document.querySelector('main') || document.querySelector('div[role="main"]');
      if (mainEl && !mainEl.hasAttribute('data-focus-blocked')) {
        mainEl.setAttribute('data-focus-blocked', 'true');
        
        mainEl.querySelectorAll('video').forEach(video => {
          try {
            video.pause();
            video.removeAttribute('src');
            video.querySelectorAll('source').forEach(src => src.removeAttribute('src'));
            video.load();
          } catch (e) {}
        });

        Array.from(mainEl.children).forEach(child => {
          if (child.querySelector('input') || child.querySelector('[role="search"]') || child.tagName === 'HEADER') {
            return;
          }
          child.hidden = true;
          child.className = '';
        });
      }
      
      // Hide any loading progress/spinners on watch page
      document.querySelectorAll('svg[aria-label="Loading..."], [role="progressbar"]').forEach(spinner => {
        spinner.hidden = true;
      });
      return;
    }

    // B. Block items on Feed (Home) page
    if (path === '/' || path === '' || path.startsWith('/home')) {
      
      // 1. Hide Stories Tray
      const storyLink = document.querySelector('a[href*="/stories/"]');
      if (storyLink) {
        const storiesContainer = findStoriesContainer(storyLink);
        if (storiesContainer && !storiesContainer.hidden) {
          storiesContainer.hidden = true;
          storiesContainer.className = '';
        }
      }

      // 2. Hide Reels navigation tab / Watch navigation tab from sidebar/menus
      document.querySelectorAll('a[href*="/reel/"], a[href*="/reels/"], a[href*="/watch/"], a[href*="/videos/"]').forEach(navLink => {
        if (navLink.closest('li') || navLink.closest('[role="navigation"]') || navLink.closest('ul')) {
          const menuItem = navLink.closest('li') || navLink;
          if (menuItem && !menuItem.hidden) {
            menuItem.hidden = true;
            menuItem.className = '';
          }
        }
      });

      // 3. Scan and Block Feed Units
      document.querySelectorAll('[aria-posinset]').forEach(card => {
        if (card.hasAttribute('data-focus-blocked')) return;

        const hasReelLink = card.querySelector('a[href*="/reel/"], a[href*="/reel?"], a[href*="/reels/"]');
        
        const cardText = card.innerText || '';
        const cardTextLower = cardText.toLowerCase();

        const isReelUnit = hasReelLink || cardTextLower.includes('reels and short videos') || cardTextLower.includes('reels & short videos');
        const isSponsored = cardText.includes('Sponsored');
        const isSuggestion = suggestionKeywords.some(kw => cardTextLower.includes(kw));

        if (isReelUnit) {
          hideAndUnload(card, 'Reel blocked');
        } else if (isSponsored) {
          hideAndUnload(card, 'Sponsored post blocked');
        } else if (isSuggestion) {
          hideAndUnload(card, 'Suggested post blocked');
        }
      });
    }
  }

  // Initial sweep
  applyBlock();

  // Watch for infinite scroll injections
  const observer = new MutationObserver(() => {
    observer.disconnect();
    try {
      applyBlock();
    } catch (e) {}
    observer.observe(document.body, { childList: true, subtree: true });
  });

  observer.observe(document.body, { childList: true, subtree: true });

  console.log("Facebook Blocker Active (Stories, Reels, Ads & Suggestions blocked)");
})();
