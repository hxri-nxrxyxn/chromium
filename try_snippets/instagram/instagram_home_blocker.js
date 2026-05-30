(function() {
  const feedKeywords = [
    'suggested for you',
    'suggested post',
    'you may like',
    'suggested accounts',
    'suggested reels',
    'sponsored'
  ];

  // Map of post types to simple blocked messages
  const focusMessages = {
    'Reel': 'Reel blocked',
    'Suggestion': 'Suggested post blocked',
    'Recommendation Module': 'Recommendation module blocked'
  };

  // Helper to hide elements and replace them with a minimalist canvas block of the same dimensions.
  // This preserves layout feed flow and page height to permanently stop infinite scroll loops
  // without using CSS styles or attributes, while keeping React nodes in the DOM to avoid crashes.
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
      const width = rect.width > 0 ? rect.width : 500;
      const height = rect.height > 0 ? rect.height : 450;

      // 3. Hide all original children elements
      Array.from(element.children).forEach(child => {
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
        
        const message = focusMessages[type] || 'Blocked';
        ctx.fillText(message, width / 2, height / 2);
      }
      
      element.appendChild(canvas);
    }
  }

  function applyBlock() {
    const path = window.location.pathname;
    if (path === '/' || path === '') {
      
      // 1. Hide Stories Tray at the top
      const storyLink = document.querySelector('a[href^="/stories/"]');
      if (storyLink) {
        const storyContainer = storyLink.closest('ul') || storyLink.closest('div[style*="overflow-x"]') || storyLink.closest('div');
        if (storyContainer && storyContainer !== document.body && !storyContainer.hidden) {
          storyContainer.className = '';
          storyContainer.hidden = true;
        }
      }

      // 2. Hide Suggested Posts, Sponsored Ads, and Reels from Main Feed
      document.querySelectorAll('article').forEach(article => {
        const isReel = article.querySelector('a[href^="/reel/"], a[href^="/reels/"]');
        const text = article.textContent.toLowerCase();
        const isSuggestedOrAd = feedKeywords.some(keyword => text.includes(keyword));

        if (isReel) {
          hideAndUnload(article, 'Reel');
        } else if (isSuggestedOrAd) {
          hideAndUnload(article, 'Suggestion');
        }
      });

      // 3. Hide Sibling Suggestion Modules (carousels, recommendation grids) in feed
      document.querySelectorAll('article').forEach(article => {
        const feedList = article.parentElement;
        if (feedList) {
          Array.from(feedList.children).forEach(child => {
            if (child.tagName !== 'ARTICLE' && !child.hasAttribute('data-focus-placeholder') && child.id !== 'focus-blocker-spacer') {
              const text = child.textContent.toLowerCase();
              const isSuggestionCard = feedKeywords.some(keyword => text.includes(keyword));
              if (isSuggestionCard) {
                hideAndUnload(child, 'Recommendation Module');
              }
            }
          });
        }
      });

      // 4. Check for boundary "You're all caught up" to end spinner loading
      const allTexts = Array.from(document.querySelectorAll('span, h1, h2, h3, h4, p'));
      const hasCaughtUp = allTexts.some(textNode => {
        const text = textNode.textContent.toLowerCase().trim();
        return text === 'suggested posts' || text.includes("you're all caught up") || text.includes("you’re all caught up");
      });

      if (hasCaughtUp) {
        document.querySelectorAll('svg[aria-label="Loading..."], [role="progressbar"]').forEach(spinner => {
          spinner.hidden = true;
        });

        // Add spacer to create padding at bottom of caught-up page
        const feedColumn = document.querySelector('main') || document.querySelector('div[role="main"]');
        if (feedColumn && !document.getElementById('focus-blocker-spacer')) {
          const spacer = document.createElement('div');
          spacer.id = 'focus-blocker-spacer';
          for (let i = 0; i < 40; i++) {
            spacer.appendChild(document.createElement('br'));
          }
          feedColumn.appendChild(spacer);
        }
      }
    }
  }

  // Initial sweep
  applyBlock();

  // Watch for dynamic infinite scroll injections
  const observer = new MutationObserver(() => {
    observer.disconnect();
    try {
      applyBlock();
    } catch (e) {}
    observer.observe(document.body, { childList: true, subtree: true });
  });

  observer.observe(document.body, { childList: true, subtree: true });

  console.log("Instagram Home Blocker Active (Suggestions, Reels, Ads & Stories hidden with wireframe placeholders)");
})();
