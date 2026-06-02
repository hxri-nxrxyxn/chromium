# Git Bloat Recovery

## Symptom

After a large `gclient sync`, `.git/objects/pack/` can balloon to 60GB+ with redundant pack files. The disk fills up, preventing `gn gen` or `ninja` from writing build output.

```bash
$ du -sh checkout/src/.git/
62G     checkout/src/.git/

$ ls -lhS checkout/src/.git/objects/pack/
-r--r--r-- 1 root root  60G ... pack-<hash>.pack  # single massive pack
-r--r--r-- 1 root root 1.4G ... pack-<hash>.pack  # older pack
```

## Why it happens

`gclient sync` downloads git objects for all DEPS (dozens of repos) with full history into the main checkout's `.git/`. The sync creates a massive pack file that may contain duplicate objects from the initial shallow clone. `git gc` and `git repack -ad` both time out trying to process a 60GB pack.

## Fix: Nuke .git and re-init shallow

Building only needs the source files, not git history. The DEPS are already downloaded in `third_party/`.

```bash
docker compose run --rm builder bash -c "
  cd /checkout/src
  COMMIT=\$(git rev-parse HEAD)
  rm -rf .git
  git init
  git remote add origin https://chromium.googlesource.com/chromium/src.git
  echo \$COMMIT > .git/shallow
  git config remote.origin.partialclonefilter blob:none
  echo \"✓ .git rebuilt — commit: \$COMMIT\"
"
du -sh checkout/src/.git/  # should be < 1MB
```

**⚠️ After this:**
- `gclient sync` will NOT work (no history to delta from)
- `gn gen` and `ninja` work fine — they read source files, not git history
- The `lastchange.py` hook won't find git info, but build still works

## Prevention

- Run `gclient sync --no-history` to avoid pulling full history
- If history is needed for development, do a separate clone in `/tmp/`
