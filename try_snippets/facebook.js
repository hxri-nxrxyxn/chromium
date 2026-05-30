const style = document.createElement('style');
style.textContent = `
  /* Background color for all routes */
  body { background-color: rgb(36 37 38) !important; }

  /* Hide posts */
  [data-tracking-duration-id] { display: none !important; }

  /* Hide stories shelf */
  [data-srat="43"] { display: none !important; }
  .hscroller { display: none !important; }

  /* Hide loading skeletons / infinite scroll placeholders */
  [data-on-first-inserted-action-id] { display: none !important; }
`;
document.head.appendChild(style);

// Stop infinite scroll triggers (method 1: hide trigger elements)
document.querySelectorAll('[data-trigger-type="1"]').forEach(el => {
  el.style.display = 'none';
});

// Stop infinite scroll triggers (method 2: observe but do nothing)
const observer = new IntersectionObserver(() => {});
document.querySelectorAll('[data-marker-id]').forEach(el => observer.observe(el));

// --- Lock scroll on "/" route only ---
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

console.log("Done!");
