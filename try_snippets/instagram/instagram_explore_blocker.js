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

        // 2. Measure the original element size or fallback to viewport dimensions
        const rect = mainEl.getBoundingClientRect();
        const width = rect.width > 0 ? rect.width : (window.innerWidth > 0 ? window.innerWidth : 600);
        const height = rect.height > 50 ? rect.height : (window.innerHeight > 0 ? window.innerHeight - 100 : 500);

        // 3. Hide all original children elements (such as the explore grid and rows)
        Array.from(mainEl.children).forEach(child => {
          child.hidden = true;
          child.className = '';
        });

        // 4. Create a native HTML5 Canvas to render the wireframe placeholder
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
          ctx.setLineDash([6, 6]);
          ctx.strokeRect(10, 10, width - 20, height - 20);

          // Draw minimalist native-feeling text
          ctx.fillStyle = '#8e8e8e'; // Instagram secondary text grey
          ctx.font = '13px -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, Helvetica, Arial, sans-serif';
          ctx.textAlign = 'center';
          ctx.textBaseline = 'middle';
          ctx.fillText("Grid blocked", width / 2, height / 2);
        }
        
        mainEl.appendChild(canvas);
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

  console.log("Instagram Explore Blocker Active (Entire explore grid replaced with 'Grid blocked' placeholder)");
})();
