# Chromium Interstitial CSS Variables & Layout

When building a custom `chrome://` interstitial/block page that should look native, copy the CSS variables and layout from Chromium's real interstitial pages (`interstitial_core.css`, `interstitial_common.css`, `neterror.css`).

## Key CSS Variables

```css
body {
  --background-color: #fff;
  --error-code-color: var(--google-gray-700);
  --google-blue-50: rgb(232, 240, 254);
  --google-blue-100: rgb(210, 227, 252);
  --google-blue-300: rgb(138, 180, 248);
  --google-blue-600: rgb(26, 115, 232);
  --google-blue-700: rgb(25, 103, 210);
  --google-gray-100: rgb(241, 243, 244);
  --google-gray-300: rgb(218, 220, 224);
  --google-gray-500: rgb(154, 160, 166);
  --google-gray-50: rgb(248, 249, 250);
  --google-gray-600: rgb(128, 134, 139);
  --google-gray-700: rgb(95, 99, 104);
  --google-gray-800: rgb(60, 64, 67);
  --google-gray-900: rgb(32, 33, 36);
  --heading-color: var(--google-gray-900);
  --link-color: rgb(88, 88, 88);
  --text-color: var(--google-gray-700);
  --primary-button-fill-color: var(--google-blue-600);
  --primary-button-fill-color-active: var(--google-blue-700);
  --primary-button-text-color: #fff;
  --secondary-button-fill-color: #fff;
  --secondary-button-border-color: var(--google-gray-500);
  --secondary-button-hover-fill-color: var(--google-gray-50);
  --secondary-button-hover-border-color: var(--google-gray-600);
  --secondary-button-text-color: var(--google-gray-700);
}
```

## Layout

```css
html {
  -webkit-text-size-adjust: 100%;
  font-size: 125%;
}

.interstitial-wrapper {
  box-sizing: border-box;
  font-size: 1em;
  line-height: 1.6em;
  margin: 14vh auto 0;
  max-width: 600px;
  width: 100%;
}

.icon {
  height: 72px;
  margin: 0 0 40px;
  width: 72px;
}

h1 {
  color: var(--heading-color);
  font-size: 1.6em;
  font-weight: normal;
  line-height: 1.25em;
  margin-bottom: 16px;
  margin-top: 0;
  word-wrap: break-word;
}

.error-code {
  color: var(--error-code-color);
  font-size: .8em;
  margin-top: 12px;
  text-transform: uppercase;
}

/* "Back to safety" button */
.nav-wrapper {
  margin-top: 51px;
}
.nav-wrapper::after {
  clear: both;
  content: '';
  display: table;
  width: 100%;
}
.secondary-button {
  background: var(--secondary-button-fill-color);
  border: 1px solid var(--secondary-button-border-color);
  border-radius: 20px;
  box-sizing: border-box;
  color: var(--secondary-button-text-color);
  cursor: pointer;
  display: inline-block;
  font-size: .875em;
  padding: 8px 16px;
  text-decoration: none;
  user-select: none;
}
.secondary-button:hover {
  background: var(--secondary-button-hover-fill-color);
  border-color: var(--secondary-button-hover-border-color);
}
```

## SVG Icon for Blocked/Disabled State

CSS `content: url(...)` only works on `::before`/`::after` pseudo-elements and replaced elements like `<img>`. For a `<div>` icon, use `background-image` with an inline data: URI SVG:

```css
/* Light mode — red circle with white dash */
.icon-blocked {
  background-image: url("data:image/svg+xml,%3Csvg xmlns='http://www.w3.org/2000/svg' width='72' height='72' viewBox='0 0 72 72'%3E%3Ccircle cx='36' cy='36' r='32' fill='%23ea4335'/%3E%3Crect x='22' y='32' width='28' height='8' rx='4' fill='%23fff'/%3E%3C/svg%3E");
}
```

## Dark Mode

```css
@media (prefers-color-scheme: dark) {
  body {
    --background-color: var(--google-gray-900);
    --error-code-color: var(--google-gray-500);
    --heading-color: var(--google-gray-500);
    --link-color: var(--google-blue-300);
    --text-color: var(--google-gray-500);
  }
  .icon-blocked {
    /* Softer red with dark dash for dark bg */
    background-image: url("data:image/svg+xml,%3Csvg xmlns='http://www.w3.org/2000/svg' width='72' height='72' viewBox='0 0 72 72'%3E%3Ccircle cx='36' cy='36' r='32' fill='%23f28b82'/%3E%3Crect x='22' y='32' width='28' height='8' rx='4' fill='%23202124'/%3E%3C/svg%3E");
  }
  .secondary-button {
    background: var(--google-gray-900);
    border-color: var(--google-gray-700);
    color: var(--google-blue-300);
  }
  .secondary-button:hover {
    background: rgb(48, 51, 57);
  }
}
```

## Mobile Breakpoints

```css
@media (max-width: 700px) {
  .interstitial-wrapper { padding: 0 10%; }
}

@media (max-width: 420px) {
  .interstitial-wrapper { padding: 0 5%; margin: 7vh auto 12px; }
  h1 { font-size: 1.5em; margin-bottom: 8px; }
  .icon { margin-bottom: 5.69vh; }
}
```

## Source Files

The real styles are at these paths in the Chromium source tree:
- `components/security_interstitials/core/common/resources/interstitial_core.css`
- `components/security_interstitials/core/common/resources/interstitial_common.css`
- `components/neterror/resources/neterror.css`
