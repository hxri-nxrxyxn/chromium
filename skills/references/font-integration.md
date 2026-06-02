# Custom Font Integration (Geist)

## Files involved

| File | Change |
|------|--------|
| `res/font/geist.ttf` | New — Geist variable TTF (100-900 weight) |
| `res/font/geist_italic.ttf` | New — Geist italic variable TTF |
| `res/font/geist_family.xml` | New — Android font-family XML |
| `res/values/styles.xml` | Edit — add `android:fontFamily` to `Base.Theme.Chromium` |
| `shorts_reels_blocker.cc` | Edit — prepend `Geist,` to font-family in block page CSS |
| `distraction_blocked_ui.cc` | Edit — prepend `Geist,` to font-family in WebUI CSS |

## Step 1: Download Geist

From Vercel's GitHub releases (MIT-licensed):
```bash
curl -L -o /tmp/geist-font.zip \
  "https://github.com/vercel/geist-font/releases/download/1.8.0/geist-font-1.8.0.zip"
python3 -c "
import zipfile
with zipfile.ZipFile('/tmp/geist-font.zip') as z:
    for name in z.namelist():
        if 'variable' in name and name.endswith('.ttf'):
            z.extract(name, '/tmp/geist-extracted')
"
```

## Step 2: Place font files in Android resources

```bash
FONT_DIR=/path/to/chromium/src/chrome/android/java/res/font
mkdir -p "$FONT_DIR"

# Rename brackets out of filenames (Android doesn't like []) 
cp /tmp/geist-extracted/.../Geist[variable]/Geist[wght].ttf          "$FONT_DIR/geist.ttf"
cp /tmp/geist-extracted/.../Geist[variable]/Geist-Italic[wght].ttf   "$FONT_DIR/geist_italic.ttf"
```

## Step 3: Create font-family XML

`chrome/android/java/res/font/geist_family.xml`:
```xml
<?xml version="1.0" encoding="utf-8"?>
<font-family xmlns:android="http://schemas.android.com/apk/res/android">
    <font android:font="@font/geist" android:fontWeight="100" android:fontStyle="normal" />
    <font android:font="@font/geist" android:fontWeight="400" android:fontStyle="normal" />
    <font android:font="@font/geist" android:fontWeight="700" android:fontStyle="normal" />
    <font android:font="@font/geist_italic" android:fontWeight="100" android:fontStyle="italic" />
    <font android:font="@font/geist_italic" android:fontWeight="400" android:fontStyle="italic" />
    <font android:font="@font/geist_italic" android:fontWeight="700" android:fontStyle="italic" />
</font-family>
```

## Step 4: Apply to base theme

In `chrome/android/java/res/values/styles.xml`, add to `Base.Theme.Chromium`:
```xml
<style name="Base.Theme.Chromium" parent="Theme.BrowserUI.DayNight">
    ...
    <item name="android:fontFamily">@font/geist_family</item>
</style>
```

All themes (`Activity`, `TabbedMode`, `Settings`, `SearchActivity`, `DialogWhenLarge`) inherit from this, so one change applies everywhere.

## Step 5: Update WebUI CSS

Block pages in `shorts_reels_blocker.cc` and `distraction_blocked_ui.cc` have inline CSS with the standard system font stack. Prepend `Geist,`:
```css
font-family:Geist,-apple-system,BlinkMacSystemFont,"Segoe UI",...;
```

Simple sed replacement:
```bash
sed -i 's|font-family:-apple-system|font-family:Geist,-apple-system|g' \
  chrome/browser/navigation_policy/shorts_reels_blocker.cc \
  chrome/browser/ui/webui/distraction_blocked/distraction_blocked_ui.cc
```

## Step 6: Register fonts in chrome_java_resources.gni (MANDATORY)

**Without this step, the build will fail:** `"Found files not listed in the sources list"`.

In `chrome/android/chrome_java_resources.gni`, find the existing `reader_mode_lexend.xml` entry and add the font files right after it:

```gni
  "java/res/font/reader_mode_lexend.xml",
  "java/res/font/geist.ttf",            # ADD
  "java/res/font/geist_italic.ttf",     # ADD
  "java/res/font/geist_mono.ttf",       # ADD
  "java/res/font/geist_mono_italic.ttf",# ADD
  "java/res/font/geist_family.xml",     # ADD
```

This file is a Chromium convention — it lists every resource file the build system needs to know about. Missing entries cause the `prepare_resources.py` gn step to fail.

## Step 7: Sync to patches

Font files + styles.xml + chrome_java_resources.gni live in `patches/source-files/chrome/android/...`. After adding to checkout, copy them there before committing.
