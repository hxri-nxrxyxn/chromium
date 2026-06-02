# Node.js DevTools Bundling — napi ABI Error

## Error transcript

```
Traceback (most recent call last):
  File "/checkout/src/out/Default/../../third_party/node/node.py", line 52
    RunNode(sys.argv[1:])
  File "/checkout/src/out/Default/../../third_party/node/node.py", line 47
    raise RuntimeError('Command \'%s\' failed\n%s' % (
RuntimeError: Command '/checkout/src/out/Default/../../third_party/node/linux/node-linux-x64/bin/node
  ../../third_party/devtools-frontend/src/node_modules/rollup/dist/bin/rollup
  --config ../../third_party/devtools-frontend/src/front_end/Images/rollup.config.mjs
  --input gen/third_party/devtools-frontend/src/front_end/Images/Images.prebundle.js
  --file gen/third_party/devtools-frontend/src/front_end/Images/Images.js' failed

[!] (plugin rollup-plugin-import-meta-assets) Error: Failed to convert napi value into rust type `bool`
```

Also appears in rollup's own native module without the plugin:

```
[!] Error: Failed to convert napi value into rust type `bool`
    at Module.setSource (/checkout/src/third_party/devtools-frontend/src/node_modules/rollup/dist/shared/rollup.js:16677:47)
```

## Root cause

The Chromium checkout includes a **bundled Node.js binary** in `third_party/node/linux/node-linux-x64/bin/node` (v24.12.0). The DevTools frontend's `node_modules/` contains native `.node` addons (rollup itself plus plugins) compiled via napi (Node-API). The bundled Node was compiled against a different glibc than Ubuntu 22.04's glibc 2.35. The napi FFI boundary fails at `setSource` when the Rust-backed napi module tries to convert values.

- Node version: v24.12.0 (bundled)
- Ubuntu: 22.04.5 LTS (glibc 2.35)
- No missing shared libraries (`ldd` passes clean)
- Error is at the Rust napi boundary, not a missing `.so`

## Proper fix — install Node.js 20 LTS in Docker image

Add to `Dockerfile`:

```dockerfile
# Install Node.js 20 LTS (required for DevTools bundling)
RUN curl -fsSL https://deb.nodesource.com/setup_20.x | bash - && \
    apt-get install -y nodejs && \
    rm -rf /var/lib/apt/lists/*
```

Rebuild the image:

```bash
docker compose build
```

The system Node (`/usr/bin/node`) takes PATH precedence over the bundled Chromium Node. Node 20 LTS provides a napi runtime compatible with the prebuilt rollup native modules. Verify with `node --version` inside the container before building.

## What NOT to do

1. **Do NOT stub output files** — stubbing `Images.js` etc. requires touching many DevTools targets, creates dead touch-points, and makes the build unmaintainable. The stubs break when Chromium updates rollup configs.
2. **Do NOT delete node_modules packages** — removing `@web/rollup-plugin-import-meta-assets` breaks other DevTools bundling steps that depend on it transitively.
3. **Do NOT modify rollup configs** — `rollup.config.mjs` is upstream Chromium code. Modifying it creates merge conflicts on every stable update.

## If Node install alone doesn't fix it

In rare cases the napi module itself was compiled with a Rust napi version incompatible with Node 20. If the error persists after Dockerfile+rebuild, the fallback is to disable DevTools entirely via gn args (the `enable_devtools` variable, if available for the target branch), or rebuild the DevTools node_modules from source inside the container using the system Node and npm.
