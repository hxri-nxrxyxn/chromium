# Chromium Network Issues & Rate Limiting

## HTTP 429 — Rate limit exceeded

Google's git servers (chromium.googlesource.com) aggressively rate-limit by IP. Symptoms:

```
remote: RESOURCE_EXHAUSTED: Resource has been exhausted (e.g. check quota)
remote: [type.googleapis.com/google.rpc.QuotaFailure]
remote: violations {
remote:   subject: "ip/103.154.37.51"
remote:   description: "Short term server-time rate limit exceeded"
remote: }
fatal: unable to access '...': The requested URL returned error: 429
```

### Workarounds

1. **Wait 60s then retry** — short-term limits clear quickly
2. **Reduce concurrency** — use `gclient sync --jobs 1` instead of default parallelism
3. **Break up operations** — fetch the main repo first, then sync deps separately

## gclient sync is SLOW — DO NOT KILL prematurely

**THE CARDINAL RULE:** "STALL DETECTED" from gclient is NORMAL for large Chromium fetches. `git fetch` produces no output while downloading, and gclient interprets silence as a stall. The process is almost certainly alive and working.

### How to check if it's REALLY dead (do this BEFORE killing)

```bash
# Check 1: Is git index-pack running? If yes, data is flowing — just slow.
ps aux | grep "git.*index-pack" | grep -v grep
# CPU > 0% means it's processing downloaded objects.

# Check 2: Is disk growing?
du -sh /home/hari/chromium-android/checkout/
# Run twice 30s apart — growth means it's working.

# Check 3: Is git fetch running?
ps aux | grep "git.*fetch" | grep -v grep
# Even at 0% CPU, git fetch can be waiting on network (1-2 MiB/s is common).
```

If ANY of these show activity, **let it run.** A 1-2 MiB/s fetch with millions of objects takes 30-60 minutes — that's normal, not dead.

### When it's ACTUALLY dead

All three checks are negative (no git processes, no disk growth for 5+ minutes) AND there's an explicit HTTP 429 error in the output:

```
remote: RESOURCE_EXHAUSTED: Resource has been exhausted
fatal: unable to access '...': The requested URL returned error: 429
```

Only then is it safe to kill and retry.

### Common speed observations

| Speed | Objects | ETA |
|-------|---------|-----|
| 1-2 MiB/s | 1.2M objects | 30-60 min per repo |
| 10+ MiB/s | 1.2M objects | 5-10 min per repo |

`gclient sync` processes multiple repos sequentially. A total sync can take 1-2 hours at slow speeds.

### Stalled gclient sync — when it IS actually dead

Rare case: git fetch exits but gclient hangs. Symptoms: no git processes in `ps aux`, no disk growth, log stuck on "Still working on: src" for 10+ minutes. In this case:

When the main clone already contains the needed commit (common when main branch HEAD ≈ stable tag):

```bash
# 1. Verify commit match
docker compose run --rm builder bash -c "
  cd /checkout/src
  git log --oneline HEAD -1          # current HEAD
  cat .git/FETCH_HEAD                # tag commit (if fetched earlier)
"

# 2. If they match, just tag and sync deps
docker compose run --rm builder bash -c "
  cd /checkout/src
  git tag -f 149.0.7827.84 HEAD
  gclient sync --nohooks --jobs 1
"
```

### Workaround: shallow tag fetch

If the commit doesn't match, fetch just the tag with shallow depth:

```bash
docker compose run --rm builder bash -c "
  cd /checkout/src
  git fetch origin refs/tags/149.0.7827.84 --depth=1   # shallow — lands in FETCH_HEAD
  git checkout FETCH_HEAD                              # checkout the tag commit
  git tag -f 149.0.7827.84 HEAD                       # create local tag ref
  gclient sync --nohooks --jobs 1                     # sync deps at low concurrency
"
```

**Note:** `--depth=1` fetches don't create local tag refs — only `FETCH_HEAD`. Must checkout `FETCH_HEAD` then tag manually. The full `git fetch origin --tags` (without depth) creates local refs but is slower and more likely to trigger rate limits.

## Docker permission issues with checkout operations

Docker bind mounts create root-owned files. Any Python script that writes to or modifies files in the checkout MUST run inside Docker as root:

```bash
# ✅ Works — runs inside container as root
docker compose run --rm -v $(pwd)/patches:/patches builder \
  python3 /patches/generate-patches.py /checkout/src /patches

# ❌ Fails — host user can't write to root-owned files
python3 patches/generate-patches.py checkout/src patches/
# PermissionError: [Errno 13] Permission denied: '/home/hari/.../SigninButtonCoordinator.java'
```
