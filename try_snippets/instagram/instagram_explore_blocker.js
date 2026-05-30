(function() {
  // Helper to hide elements and replace them with a minimalist canvas block of the same dimensions.
  // This preserves layout grids permanently to stop infinite scroll triggers,
  // using pure canvas rendering to stay CSS/style-attribute free.
  function hideAndUnload(element, type) {
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

      // 2. Measure the original element size to match the placeholder rectangle
      const rect = element.getBoundingClientRect();
      const width = rect.width > 0 ? rect.width : 250;
      const height = rect.height > 0 ? rect.height : 250;

      // 3. Hide all original children elements
      Array.from(element.children).forEach(child => {
        child.hidden = true;
        child.className = '';
      });

      // 4. Create a native HTML5 Canvas to render the minimalist block
      const canvas = document.createElement('canvas');
      canvas.setAttribute('data-focus-placeholder', 'true');
      canvas.width = width;
      canvas.height = height;
      
      const ctx = canvas.getContext('2d');
      if (ctx) {
        // Draw clean white background matching Instagram
        ctx.fillStyle = '#ffffff';
        ctx.fillRect(0, 0, width, height);

        // Draw subtle dashed border inset by 10px
        ctx.strokeStyle = '#dbdbdb'; // Standard Instagram border grey
        ctx.lineWidth = 1;
        ctx.setLineDash([5, 5]);
        ctx.strokeRect(10, 10, width - 20, height - 20);

        // Draw minimalist, intentional text
        ctx.fillStyle = '#8e8e8e'; // Instagram secondary text grey
        ctx.font = '12px -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, Helvetica, Arial, sans-serif';
        ctx.textAlign = 'center';
        ctx.textBaseline = 'middle';
        ctx.fillText("Explore grid blocked. Keep it intentional.", width / 2, height / 2);
      }
      
      element.appendChild(canvas);
    }
  }

  function applyBlock() {
    if (window.location.pathname.startsWith('/explore')) {
      const mainEl = document.querySelector('main') || document.querySelector('div[role="main"]');
      if (mainEl) {
        // Target only the feed post items and reels in the explore grid
        const postSelectors = 'a[href^="/p/"], a[href^="/reel/"], a[href^="/reels/"]';
        mainEl.querySelectorAll(postSelectors).forEach(link => {
          const gridItem = link.closest('div') || link;
          hideAndUnload(gridItem, 'Explore Post');
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

  // Watch for dynamic infinite scroll cell injections
  const observer = new MutationObserver(() => {
    observer.disconnect();
    try {
      applyBlock();
    } catch (e) {}
    observer.observe(document.body, { childList: true, subtree: true });
  });

  observer.observe(document.body, { childList: true, subtree: true });

  console.log("Instagram Explore Blocker Active (Explore grid replaced with wireframe blocks)");
})();
