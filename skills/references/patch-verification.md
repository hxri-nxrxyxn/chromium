# Patch Verification

Before writing custom patches (e.g., navigation throttles), verify the actual source to ensure you're targeting the right files and using the correct API.

## Check registration file

```bash
grep -n "AddThrottle\|CreateAndAdd\|NavigationThrottleRegistry" \
  chrome/browser/chrome_content_browser_client_navigation_throttles.cc | head -20
```

Look for the function signature and existing throttle registrations to match the pattern.

## Check BUILD.gn sources

```bash
grep -n "navigation_throttles" chrome/browser/BUILD.gn
```

Find which source_set/static_library the registration file belongs to.

## Check NavigationThrottle API

```bash
grep -n "GetNameForLogging\|WillStartRequest\|ThrottleCheckResult" \
  content/public/browser/navigation_throttle.h | head -20
```

Verify method signatures — especially `const` qualifiers.
