#!/usr/bin/env bash
#
# End-to-end test: shadowsocks-android on Android emulator
#
# Boots an emulator, starts ssserver on the host, installs the debug APK,
# modifies the default profile via run-as + sqlite3 to point to our server,
# taps the FAB to connect VPN, then verifies connectivity.
#
set -euo pipefail

# ── Paths ───────────────────────────────────────────────────────────────────
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
EMULATOR="${EMULATOR:-/Volumes/Data/workspace/android/emulator/emulator}"
ADB="${ADB:-/Volumes/Data/workspace/android/platform-tools/adb}"
AVD="${AVD:-Medium_Phone_API_36.1}"
APK="${APK:-$SCRIPT_DIR/mobile/build/outputs/apk/debug/mobile-debug.apk}"
SSSERVER="${SSSERVER:-$SCRIPT_DIR/core/src/main/rust/shadowsocks-rust/target/release/ssserver}"
PKG="com.github.shadowsocks"

# ── SS config ───────────────────────────────────────────────────────────────
SS_ADDR="0.0.0.0:8388"
SS_PASSWORD="testpassword123"
SS_METHOD="aes-256-gcm"
SS_HOST_FROM_EMU="10.0.2.2"
SS_PORT=8388

# ── Cleanup trap ────────────────────────────────────────────────────────────
SSSERVER_PID=""
cleanup() {
    echo ""
    echo "=== Cleanup ==="
    if [[ -n "$SSSERVER_PID" ]] && kill -0 "$SSSERVER_PID" 2>/dev/null; then
        echo "Killing ssserver (PID $SSSERVER_PID)"
        kill "$SSSERVER_PID" 2>/dev/null || true
        wait "$SSSERVER_PID" 2>/dev/null || true
    fi
    if [[ "${SKIP_EMULATOR_BOOT:-}" != "true" ]] && "$ADB" get-state &>/dev/null; then
        echo "Shutting down emulator..."
        "$ADB" emu kill 2>/dev/null || true
    fi
    echo "Cleanup done."
}
trap cleanup EXIT

# ── Helpers ─────────────────────────────────────────────────────────────────
fail() { echo "FAIL: $*" >&2; exit 1; }
info() { echo "--- $*"; }

wait_for_boot() {
    info "Waiting for emulator to boot..."
    "$ADB" wait-for-device
    local n=0
    while [[ $n -lt 120 ]]; do
        local val
        val=$("$ADB" shell getprop sys.boot_completed 2>/dev/null | tr -d '\r\n')
        if [[ "$val" == "1" ]]; then
            info "Emulator booted."
            return 0
        fi
        sleep 2
        n=$((n + 2))
    done
    fail "Emulator did not boot within 120s"
}

screenshot() {
    local name="$1"
    "$ADB" shell screencap -p /sdcard/screen_${name}.png 2>/dev/null || true
    "$ADB" pull /sdcard/screen_${name}.png "$SCRIPT_DIR/screen_${name}.png" 2>/dev/null || true
    info "  Screenshot saved: screen_${name}.png"
}

# ────────────────────────────────────────────────────────────────────────────
# Step 1: Verify prerequisites
# ────────────────────────────────────────────────────────────────────────────
info "Step 1: Verify prerequisites"
[[ -f "$SSSERVER" ]] || command -v "$SSSERVER" &>/dev/null || fail "ssserver not found at $SSSERVER"
[[ -f "$APK" ]]      || fail "APK not found at $APK"
[[ "${SKIP_EMULATOR_BOOT:-}" == "true" ]] || [[ -x "$EMULATOR" ]] || command -v "$EMULATOR" &>/dev/null || fail "Emulator not found at $EMULATOR"
[[ -x "$ADB" ]]      || command -v "$ADB" &>/dev/null      || fail "adb not found at $ADB"
info "All prerequisites OK."

# ────────────────────────────────────────────────────────────────────────────
# Step 2: Start ssserver
# ────────────────────────────────────────────────────────────────────────────
info "Step 2: Starting ssserver on $SS_ADDR ..."
"$SSSERVER" -s "$SS_ADDR" -k "$SS_PASSWORD" -m "$SS_METHOD" -U &
SSSERVER_PID=$!
sleep 1
kill -0 "$SSSERVER_PID" 2>/dev/null || fail "ssserver failed to start"
info "ssserver running (PID $SSSERVER_PID)"

# ────────────────────────────────────────────────────────────────────────────
# Step 3: Boot emulator
# ────────────────────────────────────────────────────────────────────────────
if [[ "${SKIP_EMULATOR_BOOT:-}" == "true" ]]; then
    info "Step 3: Skipping emulator boot (SKIP_EMULATOR_BOOT=true)"
    "$ADB" wait-for-device
    info "  Emulator already running."
else
    info "Step 3: Booting emulator ($AVD) ..."
    "$EMULATOR" -avd "$AVD" -no-snapshot-load -no-audio -gpu auto &
    wait_for_boot
    sleep 5
    "$ADB" shell input keyevent KEYCODE_HOME
    sleep 2
fi

# Disable animations for reliable UI automation
"$ADB" shell settings put global window_animation_scale 0
"$ADB" shell settings put global transition_animation_scale 0
"$ADB" shell settings put global animator_duration_scale 0

# ────────────────────────────────────────────────────────────────────────────
# Step 4: Install APK
# ────────────────────────────────────────────────────────────────────────────
info "Step 4: Installing debug APK ..."
# Uninstall any existing version first (release vs debug signatures differ)
"$ADB" uninstall "$PKG" 2>/dev/null || true
"$ADB" install -g "$APK" || fail "APK install failed"
info "APK installed."

# ────────────────────────────────────────────────────────────────────────────
# Step 5: Configure server profile via run-as + sqlite3
# ────────────────────────────────────────────────────────────────────────────
info "Step 5: Configuring profile..."

# 5a. Launch app once to initialize databases.
# ensureNotEmpty() creates a default profile (id=1) and sets profileId=1.
# serviceMode defaults to "vpn".
info "  Launching app to initialize databases..."
"$ADB" shell am start -W -n "$PKG/.MainActivity"
sleep 8
screenshot "01_init"
# Force a checkpoint to flush WAL into main database file
"$ADB" shell am force-stop "$PKG"
sleep 2

# 5b. Update the default profile (id=1) to point to our test server.
# Profile table columns: id, name, host, remotePort, password, method, ...
# With debug build, we can use run-as to copy the database out, modify it, and copy it back.
info "  Updating default profile via sqlite3..."

# Extract profile.db and WAL/SHM files using exec-out (binary-safe)
"$ADB" exec-out run-as "$PKG" cat databases/profile.db > /tmp/profile.db
"$ADB" exec-out run-as "$PKG" cat databases/profile.db-wal > /tmp/profile.db-wal 2>/dev/null || true
"$ADB" exec-out run-as "$PKG" cat databases/profile.db-shm > /tmp/profile.db-shm 2>/dev/null || true

# Verify and checkpoint WAL into main DB
file /tmp/profile.db
sqlite3 /tmp/profile.db "PRAGMA wal_checkpoint(TRUNCATE);" 2>/dev/null || true

# Check tables
info "  Tables in profile.db:"
sqlite3 /tmp/profile.db ".tables" | while IFS= read -r line; do
    info "    $line"
done

if ! sqlite3 /tmp/profile.db "SELECT count(*) FROM Profile;" >/dev/null 2>&1; then
    fail "Profile table not found — database may not have been initialized"
fi

# Modify the profile using host sqlite3
sqlite3 /tmp/profile.db "UPDATE Profile SET host='$SS_HOST_FROM_EMU', remotePort=$SS_PORT, password='$SS_PASSWORD', method='$SS_METHOD', name='Test Server' WHERE id=1;"

# Verify update
info "  Verifying profile update..."
sqlite3 /tmp/profile.db "SELECT id, name, host, remotePort, method FROM Profile;" | while IFS= read -r line; do
    info "    Profile: $line"
done

# Push modified database back (without WAL — clean state)
rm -f /tmp/profile.db-wal /tmp/profile.db-shm
"$ADB" push /tmp/profile.db /data/local/tmp/profile.db
"$ADB" shell "cat /data/local/tmp/profile.db | run-as $PKG sh -c 'cat > databases/profile.db'"
# Remove old WAL/SHM so Room starts fresh
"$ADB" shell "run-as $PKG rm -f databases/profile.db-wal databases/profile.db-shm"
"$ADB" shell rm /data/local/tmp/profile.db

info "  Profile configuration done."

# ────────────────────────────────────────────────────────────────────────────
# Step 6: Enable VPN
# ────────────────────────────────────────────────────────────────────────────
info "Step 6: Enabling VPN..."

# Launch the app
"$ADB" shell am start -W -n "$PKG/.MainActivity"
sleep 3
screenshot "02_app_launched"

# Get screen dimensions
SCREEN_SIZE=$("$ADB" shell wm size | grep -oE '[0-9]+x[0-9]+' | tail -1)
SCREEN_W=$(echo "$SCREEN_SIZE" | cut -dx -f1)
SCREEN_H=$(echo "$SCREEN_SIZE" | cut -dx -f2)
info "  Screen: ${SCREEN_W}x${SCREEN_H}"

# Tap the FAB (connect button) — centered horizontally, near bottom.
# On 1080x2400 @420dpi the FAB center is at ~93.5% of screen height.
FAB_X=$((SCREEN_W / 2))
FAB_Y=$((SCREEN_H * 93 / 100))
info "  Tapping FAB at ($FAB_X, $FAB_Y)..."
"$ADB" shell input tap "$FAB_X" "$FAB_Y"
sleep 2
screenshot "03_after_fab_tap"

# Handle VPN consent dialog
info "  Checking for VPN consent dialog..."
VPN_ACCEPTED=false
for i in $(seq 1 15); do
    ACTIVITIES=$("$ADB" shell dumpsys activity activities 2>/dev/null || true)
    if echo "$ACTIVITIES" | grep -qi "vpndialogs\|com.android.vpndialogs"; then
        info "  VPN consent dialog detected, accepting..."
        screenshot "04_vpn_dialog"
        sleep 1

        # Use uiautomator dump to find the exact OK button coordinates
        "$ADB" shell uiautomator dump /sdcard/ui_dump.xml 2>/dev/null || true
        "$ADB" pull /sdcard/ui_dump.xml /tmp/ui_dump.xml 2>/dev/null || true
        UI_XML=$(cat /tmp/ui_dump.xml 2>/dev/null || true)
        info "  UI dump obtained (${#UI_XML} chars)"

        # Log all button texts found for debugging
        BUTTONS=$(echo "$UI_XML" | tr '>' '\n' | grep -o 'text="[^"]*".*bounds="[^"]*"' || true)
        info "  Buttons found:"
        echo "$BUTTONS" | while IFS= read -r line; do
            [[ -n "$line" ]] && info "    $line"
        done

        # Extract OK button bounds from XML: text="OK" ... bounds="[x1,y1][x2,y2]"
        # Search for text="OK" or text="Allow" or text="확인" (various system locales)
        OK_LINE=$(echo "$UI_XML" | tr '>' '\n' | grep -E 'text="OK"|text="Allow"' | head -1 || true)
        if [[ -n "$OK_LINE" ]]; then
            OK_BOUNDS=$(echo "$OK_LINE" | grep -o 'bounds="\[[0-9]*,[0-9]*\]\[[0-9]*,[0-9]*\]"' || true)
            info "  OK button bounds: $OK_BOUNDS"
            if [[ -n "$OK_BOUNDS" ]]; then
                # Parse [x1,y1][x2,y2]
                NUMS=$(echo "$OK_BOUNDS" | grep -o '[0-9]*')
                X1=$(echo "$NUMS" | sed -n '1p')
                Y1=$(echo "$NUMS" | sed -n '2p')
                X2=$(echo "$NUMS" | sed -n '3p')
                Y2=$(echo "$NUMS" | sed -n '4p')
                TAP_X=$(( (X1 + X2) / 2 ))
                TAP_Y=$(( (Y1 + Y2) / 2 ))
                info "  Tapping OK at ($TAP_X, $TAP_Y)..."
                "$ADB" shell input tap "$TAP_X" "$TAP_Y"
                sleep 2
            fi
        else
            info "  OK button not found in UI dump, trying coordinate-based tap..."
            # From screenshot analysis: OK button is at ~82% x, ~59% y on 1080x2400
            OK_TAP_X=$((SCREEN_W * 82 / 100))
            OK_TAP_Y=$((SCREEN_H * 59 / 100))
            info "  Tapping estimated OK at ($OK_TAP_X, $OK_TAP_Y)..."
            "$ADB" shell input tap "$OK_TAP_X" "$OK_TAP_Y"
            sleep 2
        fi

        # Fallback: if dialog still showing, try keyboard approach
        if "$ADB" shell dumpsys activity activities 2>/dev/null | grep -qi "vpndialogs"; then
            info "  Dialog still showing, trying DPAD_RIGHT + ENTER..."
            "$ADB" shell input keyevent KEYCODE_DPAD_RIGHT
            sleep 0.3
            "$ADB" shell input keyevent KEYCODE_ENTER
            sleep 2
        fi

        # Second fallback: try TAB + ENTER
        if "$ADB" shell dumpsys activity activities 2>/dev/null | grep -qi "vpndialogs"; then
            info "  Dialog still showing, trying TAB + ENTER..."
            "$ADB" shell input keyevent KEYCODE_TAB
            sleep 0.3
            "$ADB" shell input keyevent KEYCODE_TAB
            sleep 0.3
            "$ADB" shell input keyevent KEYCODE_ENTER
            sleep 2
        fi

        VPN_ACCEPTED=true
        screenshot "05_after_vpn_accept"
        break
    fi
    sleep 1
done

if [[ "$VPN_ACCEPTED" != "true" ]]; then
    info "  No VPN consent dialog detected"
    screenshot "04_no_vpn_dialog"
fi

# ────────────────────────────────────────────────────────────────────────────
# Step 7: Verify VPN is connected
# ────────────────────────────────────────────────────────────────────────────
info "Step 7: Verifying VPN connection..."
sleep 8
screenshot "06_vpn_status"

VPN_UP=false

# Check tun0 interface (must exist for VPN to be working)
info "  Checking tun0 interface..."
TUN_CHECK=$("$ADB" shell ip addr show tun0 2>&1 || true)
if echo "$TUN_CHECK" | grep -q "inet "; then
    info "  tun0 interface exists with IP address."
    echo "$TUN_CHECK"
    VPN_UP=true
else
    echo "$TUN_CHECK"
    info "  tun0 not found — VPN is NOT connected."
    info "  Dumping relevant logcat..."
    "$ADB" logcat -d 2>/dev/null | grep -iE "shadowsocks|vpn|sslocal|tun|StartService" | tail -40 || true
fi

# Check service
info "  Checking shadowsocks service..."
SVC_CHECK=$("$ADB" shell dumpsys activity services "$PKG" 2>&1 || true)
if echo "$SVC_CHECK" | grep -qi "shadowsocks\|VpnService"; then
    info "  Shadowsocks service is running."
else
    info "  WARNING: Could not confirm service state"
fi

# Dump sslocal-related logcat for debugging
info "  Recent sslocal/VPN logcat:"
"$ADB" logcat -d 2>/dev/null | grep -iE "shadowsocks|sslocal|ssservice|vpn|tun" | tail -20 || true

# ────────────────────────────────────────────────────────────────────────────
# Step 8: Test connectivity from inside the emulator
# ────────────────────────────────────────────────────────────────────────────
info "Step 8: Testing connectivity through VPN..."

PASS=0
TOTAL=4

# Test 1: VPN tunnel must be up
info "  Test 1: VPN tunnel (tun0) is active..."
if [[ "$VPN_UP" == "true" ]]; then
    info "  PASS: tun0 exists"
    PASS=$((PASS + 1))
else
    echo "  FAIL: tun0 does not exist — VPN never connected"
fi

# Test 2: DNS resolution via ping (ping resolves hostname even if ICMP is dropped)
info "  Test 2: DNS resolution (ping -c1 google.com)..."
DNS_OUT=$("$ADB" shell "ping -c 1 -W 5 google.com 2>&1" || true)
if echo "$DNS_OUT" | grep -qE "PING google\.com \([0-9]+\.[0-9]+"; then
    info "  PASS: DNS resolution succeeded (hostname resolved)"
    echo "$DNS_OUT" | head -1
    PASS=$((PASS + 1))
else
    echo "  FAIL: DNS resolution failed"
    echo "$DNS_OUT"
fi

# Test 3: TCP connect to 1.1.1.1:80 (proves TCP tunneling works)
# Note: toybox nc on some Android versions lacks -z; use "echo | nc" instead.
info "  Test 3: TCP connect to 1.1.1.1:80..."
NC1_EXIT=$("$ADB" shell "echo '' | nc -w 5 1.1.1.1 80 >/dev/null 2>&1; echo \$?" | tr -d '\r' | tail -1)
if [[ "$NC1_EXIT" == "0" ]]; then
    info "  PASS: TCP connect to 1.1.1.1:80 succeeded"
    PASS=$((PASS + 1))
else
    echo "  FAIL: TCP connect to 1.1.1.1:80 failed (exit=$NC1_EXIT)"
fi

# Test 4: TCP connect to a different host (confirms full routing)
info "  Test 4: TCP connect to 8.8.8.8:443..."
NC2_EXIT=$("$ADB" shell "echo '' | nc -w 5 8.8.8.8 443 >/dev/null 2>&1; echo \$?" | tr -d '\r' | tail -1)
if [[ "$NC2_EXIT" == "0" ]]; then
    info "  PASS: TCP connect to 8.8.8.8:443 succeeded"
    PASS=$((PASS + 1))
else
    echo "  FAIL: TCP connect to 8.8.8.8:443 failed (exit=$NC2_EXIT)"
fi

# ────────────────────────────────────────────────────────────────────────────
# Summary
# ────────────────────────────────────────────────────────────────────────────
echo ""
echo "========================================"
echo "  E2E Test Results: $PASS/$TOTAL passed"
echo "========================================"
if [[ $PASS -eq $TOTAL ]]; then
    echo "  ALL TESTS PASSED"
    exit 0
else
    echo "  SOME TESTS FAILED"
    exit 1
fi
