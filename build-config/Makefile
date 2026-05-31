# Chromium Android Builder — Makefile
#
# Commands:
#   make setup       — clone depot_tools + build Docker image
#   make fetch       — fetch Chromium source (~30 min, ~15-30GB)
#   make patches     — apply custom patches (e.g. shorts/reels blocker)
#   make deps        — install build dependencies + run hooks
#   make configure   — generate Ninja files with GN args (interactive)
#   make build       — build chrome_public_apk (hours)
#   make shell       — interactive shell inside the container
#   make install     — install APK to connected Android device
#   make unpatches   — revert custom patches
#   make clean       — remove container + volumes (keeps source)
#   make nuke        — remove everything incl. source + Docker image

BUILD_DIR ?= out/Default

all: build

# ── Phase 0: Setup ──────────────────────────────────────────────────────

.PHONY: setup
setup: depot_tools image

depot_tools:
	git clone https://chromium.googlesource.com/chromium/tools/depot_tools.git
	$(MAKE) git-config

image:
	docker compose build

# ── Phase 1: Fetch source (~30 min) ─────────────────────────────────────

LOG_DIR ?= build-logs
LOG_FILE ?= $(LOG_DIR)/fetch.log

.PHONY: fetch
fetch: | $(LOG_DIR)
	@mkdir -p checkout
	@echo "━━━ Fetching Chromium source for Android ━━━" | tee -a $(LOG_FILE)
	@echo "     Log: $(abspath $(LOG_FILE))" | tee -a $(LOG_FILE)
	docker compose run --rm builder bash -c "\
		cd /checkout && \
		fetch --nohooks --no-history android 2>&1 \
	" | tee -a $(LOG_FILE)
	@echo "✓ Source fetched to checkout/" | tee -a $(LOG_FILE)

$(LOG_DIR):
	@mkdir -p $(LOG_DIR)

# ── Phase 1b: Apply custom patches ──────────────────────────────────────

.PHONY: patches
patches: checkout/src/.git
	@echo "━━━ Applying custom patches ━━━"
	python3 patches/apply-patches.py "$(PWD)/checkout/src" "$(PWD)/patches"
	@echo "✓ All patches applied"

.PHONY: unpatches
unpatches:
	@echo "━━━ Reverting patches ━━━"
	cd checkout/src && git checkout -- . 2>/dev/null; true
	rm -f checkout/src/chrome/browser/navigation_policy/shorts_reels_blocker.h
	rm -f checkout/src/chrome/browser/navigation_policy/shorts_reels_blocker.cc
	@echo "✓ Patches reverted"

# ── Phase 2: Install dependencies + hooks ───────────────────────────────

LOG_FILE_DEPS ?= $(LOG_DIR)/deps.log

.PHONY: deps
deps: install-deps run-hooks | $(LOG_DIR)

.PHONY: install-deps
install-deps: | $(LOG_DIR)
	@echo "━━━ Installing build dependencies ━━━" | tee -a $(LOG_FILE_DEPS)
	docker compose run --rm builder bash -c "\
		./build/install-build-deps.sh 2>&1 \
	" | tee -a $(LOG_FILE_DEPS)
	@echo "✓ Build dependencies installed" | tee -a $(LOG_FILE_DEPS)

.PHONY: run-hooks
run-hooks: | $(LOG_DIR)
	@echo "━━━ Running hooks ━━━" | tee -a $(LOG_FILE_DEPS)
	docker compose run --rm builder \
		gclient runhooks 2>&1 | tee -a $(LOG_FILE_DEPS)
	@echo "✓ Hooks complete" | tee -a $(LOG_FILE_DEPS)

# ── Phase 3: Configure build ────────────────────────────────────────────

.PHONY: configure
configure:
	@echo "━━━ Generating Ninja build files ━━━"
	@echo "Args: target_os=android, target_cpu=arm64, symbol_level=1, blink/v8=0, is_debug=false"
	mkdir -p checkout/src/out/Default
	printf '%s\n' \
		'target_os = "android"' \
		'target_cpu = "arm64"' \
		'symbol_level = 1' \
		'blink_symbol_level = 0' \
		'v8_symbol_level = 0' \
		'is_debug = false' \
		'treat_warnings_as_errors = false' \
		'android_static_analysis = "off"' \
		> checkout/src/out/Default/args.gn
	docker compose run --rm builder bash -c 'gn gen /checkout/src/out/Default'
	@echo "✓ Build configured in $(BUILD_DIR)"

.PHONY: reconfigure
reconfigure:
	@echo "━━━ Reconfiguring Ninja build files ━━━"
	docker compose run --rm builder bash -c "\
		gn gen $(BUILD_DIR) --args='$(ARGS)' \
	"
	@echo "✓ Build reconfigured"

LOG_FILE_BUILD ?= $(LOG_DIR)/build.log

# ── Phase 4: Build ──────────────────────────────────────────────────────

.PHONY: build
build: | $(LOG_DIR)
	@echo "━━━ Building chrome_public_apk ━━━" | tee -a $(LOG_FILE_BUILD)
	@echo "     Using ninja -j4 (4 cores). 53GB swap available. 🔥" | tee -a $(LOG_FILE_BUILD)
	@echo "     Log: $(abspath $(LOG_FILE_BUILD))" | tee -a $(LOG_FILE_BUILD)
	@echo "     ETA: ~6-10 hours. Go do something else. ☕" | tee -a $(LOG_FILE_BUILD)
	docker compose run --rm -e GCLIENT_SUPPRESS_GIT_VERSION_WARNING=1 builder \
		ninja -j4 -C $(BUILD_DIR) chrome_public_apk 2>&1 | tee -a $(LOG_FILE_BUILD)
	@echo "✓ Build complete! APK at checkout/src/$(BUILD_DIR)/apks/ChromePublic.apk" | tee -a $(LOG_FILE_BUILD)

# ── Utilities ──────────────────────────────────────────────────────────

.PHONY: shell
shell:
	docker compose run --rm builder bash

.PHONY: install
install:
	docker compose run --rm builder \
		$(BUILD_DIR)/bin/chrome_public_apk install

.PHONY: git-config
git-config:
	@echo "━━━ Setting Git safe.directory for bind mounts ━━━"
	git config --global --add safe.directory "$$(pwd)/depot_tools" || true
	git config --global --add safe.directory "$$(pwd)/checkout/src" || true

# ── Cleanup ────────────────────────────────────────────────────────────

.PHONY: clean
clean:
	@echo "━━━ Cleaning up containers ━━━"
	docker compose down -v 2>/dev/null || true
	@echo "✓ Containers removed. Source files preserved in checkout/"
	@echo "  To remove everything (incl. source + image): make nuke"

.PHONY: nuke
nuke: clean
	@echo "━━━ Removing source, depot_tools, and Docker image ━━━"
	rm -rf checkout depot_tools
	docker rmi chromium-android-builder 2>/dev/null || true
	docker system prune -f 2>/dev/null || true
	@echo "✓ Everything gone. Single rm -rf ~/chromium-android would also work."
