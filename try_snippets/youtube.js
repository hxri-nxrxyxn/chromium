(function() {
  // 1. Clean up any existing style tag from previous runs
  var existingStyle = document.getElementById('youtube-mobile-distraction-blocker-style');
  if (existingStyle) {
    existingStyle.parentNode.removeChild(existingStyle);
  }

  // 2. Define the stylesheet rule string
  var css = '';
  
  // Hide all children of the main ytm-app wrapper that are not the header-bar or pivot-bar
  css += 'ytm-app > *:not(#header-bar):not(ytm-pivot-bar-renderer):not(ytm-header-bar):not(header) { display: none !important; } ';
  
  // Hide all top-level body children except the app, header-bar, or pivot-bar
  css += 'body > *:not(ytm-app):not(#header-bar):not(ytm-pivot-bar-renderer) { display: none !important; } ';
  
  // Explicitly hide the single page app body content area, feeds, player pages, and lazy loaders
  css += 'ytm-single-page-app-body, #content, .lazy-list, ytm-browse, ytm-watch, ytm-shorts, #shorts-container { display: none !important; } ';

  // 3. Create and inject style element
  var style = document.createElement('style');
  style.id = 'youtube-mobile-distraction-blocker-style';
  style.type = 'text/css';
  if (style.styleSheet) {
    style.styleSheet.cssText = css;
  } else {
    style.appendChild(document.createTextNode(css));
  }
  document.head.appendChild(style);

  console.log('Mobile YouTube distraction blocker active!');
})();
