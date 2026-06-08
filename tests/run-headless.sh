#!/usr/bin/env bash
set -euo pipefail

ROOT=$(cd "$(dirname "$0")/.." && pwd)
BUILD="$ROOT/tests/build"
RUNTIME=""
LOG=""
ERR=""
DWL_PID=""
HEADLESS_OUTPUTS=1
PIDS=()
declare -A CLIENT_PIDS=()

cleanup_runtime() {
	for pid in "${PIDS[@]:-}"; do
		kill "$pid" 2>/dev/null || true
	done
	sleep 0.05
	for pid in "${PIDS[@]:-}"; do
		kill -KILL "$pid" 2>/dev/null || true
		wait "$pid" 2>/dev/null || true
	done
	PIDS=()
	CLIENT_PIDS=()
	if [ -n "${DWL_PID:-}" ]; then
		kill "$DWL_PID" 2>/dev/null || true
		sleep 0.05
		kill -KILL "$DWL_PID" 2>/dev/null || true
		wait "$DWL_PID" 2>/dev/null || true
		DWL_PID=""
	fi
	if [ -n "${RUNTIME:-}" ]; then
		rm -rf "$RUNTIME"
		RUNTIME=""
	fi
}
trap cleanup_runtime EXIT

build_helpers() {
	mkdir -p "$BUILD"
	wayland-scanner client-header \
		/usr/share/wayland-protocols/staging/xdg-activation/xdg-activation-v1.xml \
		"$BUILD/xdg-activation-v1-client-protocol.h"
	wayland-scanner private-code \
		/usr/share/wayland-protocols/staging/xdg-activation/xdg-activation-v1.xml \
		"$BUILD/xdg-activation-v1-protocol.c"
	wayland-scanner client-header \
		/usr/share/wayland-protocols/stable/xdg-shell/xdg-shell.xml \
		"$BUILD/xdg-shell-client-protocol.h"
	wayland-scanner private-code \
		/usr/share/wayland-protocols/stable/xdg-shell/xdg-shell.xml \
		"$BUILD/xdg-shell-protocol.c"
	wayland-scanner client-header \
		"$ROOT/protocols/wlr-layer-shell-unstable-v1.xml" \
		"$BUILD/wlr-layer-shell-unstable-v1-client-protocol.h"
	wayland-scanner private-code \
		"$ROOT/protocols/wlr-layer-shell-unstable-v1.xml" \
		"$BUILD/wlr-layer-shell-unstable-v1-protocol.c"
	wayland-scanner client-header \
		"$ROOT/wlroots/protocol/virtual-keyboard-unstable-v1.xml" \
		"$BUILD/virtual-keyboard-unstable-v1-client-protocol.h"
	wayland-scanner private-code \
		"$ROOT/wlroots/protocol/virtual-keyboard-unstable-v1.xml" \
		"$BUILD/virtual-keyboard-unstable-v1-protocol.c"
	wayland-scanner client-header \
		"$ROOT/wlroots/protocol/wlr-virtual-pointer-unstable-v1.xml" \
		"$BUILD/wlr-virtual-pointer-unstable-v1-client-protocol.h"
	wayland-scanner private-code \
		"$ROOT/wlroots/protocol/wlr-virtual-pointer-unstable-v1.xml" \
		"$BUILD/wlr-virtual-pointer-unstable-v1-protocol.c"
	gcc -Wall -Wextra -Wno-unused-parameter -Werror=implicit -I"$BUILD" \
		"$ROOT/tests/headless-client.c" "$BUILD/xdg-shell-protocol.c" \
		"$BUILD/xdg-activation-v1-protocol.c" \
		$(pkg-config --cflags --libs wayland-client) -o "$BUILD/headless-client"
	gcc -Wall -Wextra -Wno-unused-parameter -Werror=implicit -I"$BUILD" \
		"$ROOT/tests/virtual-key.c" "$BUILD/virtual-keyboard-unstable-v1-protocol.c" \
		$(pkg-config --cflags --libs wayland-client xkbcommon) -o "$BUILD/virtual-key"
	gcc -Wall -Wextra -Wno-unused-parameter -Werror=implicit -I"$BUILD" \
		"$ROOT/tests/virtual-pointer.c" "$BUILD/wlr-virtual-pointer-unstable-v1-protocol.c" \
		$(pkg-config --cflags --libs wayland-client) -o "$BUILD/virtual-pointer"
	gcc -Wall -Wextra -Wno-unused-parameter -Werror=implicit -I"$BUILD" \
		"$ROOT/tests/layer-client.c" "$BUILD/wlr-layer-shell-unstable-v1-protocol.c" \
		"$BUILD/xdg-shell-protocol.c" \
		$(pkg-config --cflags --libs wayland-client) -o "$BUILD/layer-client"
}

wait_for_socket() {
	for _ in $(seq 1 100); do
		for sock in "$RUNTIME"/wayland-*; do
			if [ -S "$sock" ]; then
				export WAYLAND_DISPLAY=$(basename "$sock")
				return 0
			fi
		done
		sleep 0.05
	done
	echo "dwl did not create a Wayland socket" >&2
	return 1
}

start_dwl() {
	cleanup_runtime
	RUNTIME=$(mktemp -d /tmp/dwl-test-runtime.XXXXXX)
	LOG=$(mktemp /tmp/dwl-test-log.XXXXXX)
	ERR=$(mktemp /tmp/dwl-test-err.XXXXXX)
	chmod 700 "$RUNTIME"
	XDG_RUNTIME_DIR="$RUNTIME" WLR_BACKENDS=headless WLR_HEADLESS_OUTPUTS="$HEADLESS_OUTPUTS" \
		WLR_LIBINPUT_NO_DEVICES=1 \
		"$ROOT/dwl" >"$LOG" 2>"$ERR" &
	DWL_PID=$!
	export XDG_RUNTIME_DIR="$RUNTIME"
	wait_for_socket
}

fail_with_logs() {
	printf '%s\n' "$1" >&2
	printf '%s\n' "--- dwl stdout ---" >&2
	tail -80 "$LOG" >&2 || true
	printf '%s\n' "--- dwl stderr ---" >&2
	tail -80 "$ERR" >&2 || true
	return 1
}

slot_rgb() {
	local ppm=$1
	local slot=$2
	local slots=$3
	python3 - "$ppm" "$slot" "$slots" <<'PY'
import sys
p, slot, slots = sys.argv[1], int(sys.argv[2]), int(sys.argv[3])
with open(p, 'rb') as f:
    assert f.readline().strip() == b'P6'
    line = f.readline()
    while line.startswith(b'#'):
        line = f.readline()
    w, h = map(int, line.split())
    assert int(f.readline()) == 255
    data = f.read()
tile = max(w // slots, 1)
x = min(slot * tile + min(80, max(tile - 2, 0)), w - 1)
y = min(80, h - 1)
idx = (y * w + x) * 3
print(data[idx:idx+3].hex())
PY
}

wait_slots() {
	local slots=$1
	shift
	local expected=("$@")
	local ppm="$BUILD/screen.ppm"
	local got=""
	for _ in $(seq 1 120); do
		grim -t ppm -o HEADLESS-1 "$ppm" 2>/dev/null || true
		if [ -s "$ppm" ]; then
			got=""
			local ok=1
			for i in "${!expected[@]}"; do
				local c
				c=$(slot_rgb "$ppm" "$i" "$slots")
				got+="${got:+ }$c"
				if ! color_matches "$c" "${expected[$i]}"; then
					ok=0
				fi
			done
			if [ "$ok" -eq 1 ]; then
				return 0
			fi
		fi
		sleep 0.05
	done
	fail_with_logs "expected slots: ${expected[*]}, got: ${got:-none}"
}

color_matches() {
	local got=$1
	local want=$2
	python3 - "$got" "$want" <<'PY'
import sys
got, want = sys.argv[1], sys.argv[2]
r, g, b = int(got[0:2], 16), int(got[2:4], 16), int(got[4:6], 16)
ok = False
if want == 'ff0000':
    ok = r > 180 and g < 100 and b < 100
elif want == '00ff00':
    ok = g > 180 and r < 100 and b < 100
elif want == '0000ff':
    ok = b > 180 and r < 100 and g < 100
elif want == 'ffff00':
    ok = r > 180 and g > 180 and b < 100
elif want == 'ff00ff':
    ok = r > 180 and b > 180 and g < 100
elif want == '00ffff':
    ok = g > 180 and b > 180 and r < 100
elif want == '000000':
    ok = r < 20 and g < 20 and b < 20
else:
    ok = got == want
sys.exit(0 if ok else 1)
PY
}

wait_color() {
	wait_slots 1 "$1"
}

output_color() {
	local output=$1
	local want=$2
	local ppm="$BUILD/$output.ppm"
	grim -t ppm -o "$output" "$ppm" 2>/dev/null || return 1
	local got
	got=$(slot_rgb "$ppm" 0 1)
	color_matches "$got" "$want"
}

spawn_client() {
	local name=$1
	local color=$2
	shift 2 || true
	"$BUILD/headless-client" --title "$name" --appid "$name" --color "$color" "$@" &
	local pid=$!
	PIDS+=("$pid")
	CLIENT_PIDS[$name]=$pid
	sleep 0.25
}

spawn_layer() {
	local name=$1
	shift
	"$BUILD/layer-client" "$@" &
	local pid=$!
	PIDS+=("$pid")
	CLIENT_PIDS[$name]=$pid
	sleep 0.25
}

wait_file() {
	local file=$1
	for _ in $(seq 1 100); do
		[ -s "$file" ] && return 0
		sleep 0.05
	done
	fail_with_logs "expected file $file to be written"
}

kill_client() {
	local name=$1
	local pid=${CLIENT_PIDS[$name]:-}
	if [ -z "$pid" ]; then
		echo "unknown client $name" >&2
		return 1
	fi
	kill "$pid" 2>/dev/null || true
	sleep 0.05
	kill -KILL "$pid" 2>/dev/null || true
	wait "$pid" 2>/dev/null || true
	unset "CLIENT_PIDS[$name]"
	sleep 0.25
}

key() {
	"$BUILD/virtual-key" "$@"
	sleep 0.20
}

click() {
	"$BUILD/virtual-pointer" "$@"
	sleep 0.20
}

spawn_abc_focus_b() {
	spawn_client A ff0000
	spawn_client B 00ff00
	key right
	wait_color 00ff00
	spawn_client C 0000ff
	wait_color 00ff00
}

spawn_abc_order_focus_a() {
	spawn_abc_focus_b
	key left
	wait_color ff0000
}

spawn_abcd_order_focus_c() {
	spawn_abc_focus_b
	key right
	wait_color 0000ff
	spawn_client D ffff00
	wait_color 0000ff
}

scenario_focus_insert_move() {
	echo "scenario: focus, no-token insertion, left/right movement"
	start_dwl
	spawn_client A ff0000
	wait_color ff0000
	spawn_client B 00ff00
	wait_color ff0000
	key right
	wait_color 00ff00
	key left
	wait_color ff0000
	spawn_client C 0000ff
	wait_color ff0000
	key right
	wait_color 0000ff
	key right
	wait_color 00ff00
}

scenario_close_fallbacks() {
	echo "scenario: close fallback focused middle"
	start_dwl
	spawn_abc_focus_b
	wait_color 00ff00
	key --shift c
	wait_color ff0000

	echo "scenario: close fallback focused first"
	start_dwl
	spawn_abc_focus_b
	key left
	wait_color ff0000
	key --shift c
	wait_color 00ff00

	echo "scenario: close fallback focused last"
	start_dwl
	spawn_abc_focus_b
	key right
	wait_color 0000ff
	key --shift c
	wait_color 00ff00

	echo "scenario: close unfocused before/after preserves focused identity"
	start_dwl
	spawn_abc_focus_b
	wait_color 00ff00
	kill_client A
	wait_color 00ff00
	kill_client C
	wait_color 00ff00

	echo "scenario: close last remaining client leaves empty output stable"
	start_dwl
	spawn_client A ff0000
	wait_color ff0000
	key --shift c
	wait_slots 1 000000
}

scenario_viewport_split() {
	echo "scenario: viewport width and no-token creation outside viewport"
	start_dwl
	spawn_client A ff0000
	spawn_client B 00ff00
	key i
	wait_slots 2 ff0000 00ff00
	key right
	wait_slots 2 ff0000 00ff00
	spawn_client C 0000ff
	wait_slots 2 ff0000 00ff00
	key right
	wait_slots 2 00ff00 0000ff
	key d
	wait_color 0000ff
}

scenario_viewport_corners() {
	echo "scenario: viewport grows to three and past client count"
	start_dwl
	spawn_abc_order_focus_a
	key i
	wait_slots 2 ff0000 00ff00
	key i
	wait_slots 3 ff0000 00ff00 0000ff
	key i
	# Growing past client count should remain stable and keep all clients visible.
	wait_slots 3 ff0000 00ff00 0000ff

	echo "scenario: shrink viewport keeps right-edge focus visible"
	start_dwl
	spawn_abcd_order_focus_c
	key left
	key left
	wait_color ff0000
	key i
	key i
	wait_slots 3 ff0000 00ff00 0000ff
	key right
	key right
	wait_slots 3 ff0000 00ff00 0000ff
	key d
	wait_slots 2 00ff00 0000ff
	key d
	wait_color 0000ff

	echo "scenario: no-token map near right edge does not scroll"
	start_dwl
	spawn_abc_order_focus_a
	key i
	wait_slots 2 ff0000 00ff00
	key right
	wait_slots 2 ff0000 00ff00
	spawn_client D ffff00
	wait_slots 2 ff0000 00ff00
	key right
	wait_slots 2 00ff00 ffff00

	echo "scenario: close before viewport preserves focused identity"
	start_dwl
	spawn_abcd_order_focus_c
	key left
	key left
	wait_color ff0000
	key i
	key right
	key right
	wait_slots 2 00ff00 0000ff
	kill_client A
	wait_slots 2 00ff00 0000ff

	echo "scenario: close focused inside viewport falls left and remains visible"
	start_dwl
	spawn_abcd_order_focus_c
	key left
	key left
	wait_color ff0000
	key i
	key right
	key right
	wait_slots 2 00ff00 0000ff
	key --shift c
	wait_slots 2 00ff00 ffff00
	key left
	wait_slots 2 ff0000 00ff00

	echo "scenario: move focused client while viewport width is two"
	start_dwl
	spawn_abc_focus_b
	key i
	wait_slots 2 00ff00 0000ff
	key --shift left
	wait_slots 2 00ff00 ff0000
	key --shift right
	wait_slots 2 ff0000 00ff00
}

scenario_virtual_desktops() {
	echo "scenario: virtual desktop remembered focus and up/down movement"
	start_dwl
	spawn_abc_focus_b
	wait_color 00ff00
	key 1
	spawn_client D ffff00
	wait_color ffff00
	key 0
	wait_color 00ff00
	key 1
	wait_color ffff00
	key 0
	wait_color 00ff00
	key down
	wait_color ffff00
	key up
	wait_color 00ff00

	echo "scenario: empty virtual desktop switch and return"
	start_dwl
	spawn_client A ff0000
	wait_color ff0000
	key 2
	wait_slots 1 000000
	key 0
	wait_color ff0000
}

scenario_move_windows() {
	echo "scenario: move focused window left/right"
	start_dwl
	spawn_abc_focus_b
	wait_color 00ff00
	key --shift left
	wait_color 00ff00
	key right
	wait_color ff0000
	key right
	wait_color 0000ff

	echo "scenario: move focused window right"
	start_dwl
	spawn_abc_focus_b
	wait_color 00ff00
	key --shift right
	wait_color 00ff00
	key left
	wait_color 0000ff
	key left
	wait_color ff0000

	echo "scenario: move focused window between virtual desktops"
	start_dwl
	spawn_client A ff0000
	spawn_client B 00ff00
	key right
	wait_color 00ff00
	key --shift down
	wait_color 00ff00
	key 0
	wait_color ff0000
	key 1
	wait_color 00ff00
}

scenario_fullscreen() {
	echo "scenario: fullscreen toggle keeps focused client"
	start_dwl
	spawn_client A ff0000
	spawn_client B 00ff00
	wait_color ff0000
	key e
	wait_color ff0000
	key e
	wait_color ff0000

	echo "scenario: fullscreen toggle between clients preserves unfocused"
	start_dwl
	spawn_client A ff0000
	spawn_client B 00ff00
	wait_color ff0000
	key e
	wait_color ff0000
	key e
	wait_color ff0000
	kill_client B
	wait_color ff0000

	echo "scenario: rapid fullscreen toggle stabilizes"
	start_dwl
	spawn_client A ff0000
	wait_color ff0000
	for _ in 1 2 3 4 5 6 7 8; do
		key e
		sleep 0.1
	done
	wait_color ff0000
	key e
	wait_color ff0000
}

scenario_activation() {
	echo "scenario: xdg activation after map rejects no-serial token"
	start_dwl
	local token_file="$BUILD/token-after"
	rm -f "$token_file"
	spawn_client A ff0000 --token-file "$token_file"
	wait_color ff0000
	wait_file "$token_file"
	local token
	token=$(cat "$token_file")
	spawn_client B 00ff00 --activate-token "$token" --activate-after-map
	wait_color ff0000

	echo "scenario: xdg activation before map rejects no-serial token"
	start_dwl
	token_file="$BUILD/token-before"
	rm -f "$token_file"
	spawn_client A ff0000 --token-file "$token_file"
	wait_color ff0000
	wait_file "$token_file"
	token=$(cat "$token_file")
	spawn_client B 00ff00 --activate-token "$token" --activate-before-map
	wait_color ff0000

	echo "scenario: xdg self activation rejects no-serial token"
	start_dwl
	spawn_client A ff0000
	wait_color ff0000
	spawn_client B 00ff00 --activate-requested-token
	wait_color ff0000
}

scenario_pointer() {
	echo "scenario: pointer click focuses visible client"
	start_dwl
	spawn_abc_focus_b
	key left
	wait_color ff0000
	key i
	wait_slots 2 ff0000 00ff00
	click 720 80
	key right
	wait_slots 2 00ff00 0000ff
}

scenario_layer_shell() {
	echo "scenario: non-keyboard layer does not steal focus"
	start_dwl
	spawn_abc_focus_b
	wait_color 00ff00
	spawn_layer L --overlay --color ffff00
	wait_color ffff00
	kill_client L
	wait_color 00ff00
	key right
	wait_color 0000ff

	echo "scenario: exclusive layer restores focused client after unmap"
	start_dwl
	spawn_abc_focus_b
	wait_color 00ff00
	spawn_layer L --overlay --exclusive --color ffff00
	wait_color ffff00
	kill_client L
	wait_color 00ff00
	key right
	wait_color 0000ff

	echo "scenario: exclusive layer restores fullscreen focused client"
	start_dwl
	spawn_client A ff0000
	spawn_client B 00ff00
	wait_color ff0000
	key e
	wait_color ff0000
	spawn_layer L --overlay --exclusive --color ffff00
	wait_color ffff00
	kill_client L
	wait_color ff0000
	key e
	wait_color ff0000
}

scenario_focus_boundaries() {
	echo "scenario: focus boundaries do not wrap"
	start_dwl
	spawn_abc_focus_b
	key left
	wait_color ff0000
	key left
	wait_color ff0000
	key right
	key right
	wait_color 0000ff
	key right
	wait_color 0000ff
}

scenario_screenshot_smoke() {
	echo "scenario: grim screenshot smoke"
	grim -o HEADLESS-1 "$BUILD/headless.png"
	test -s "$BUILD/headless.png"
}

scenario_multioutput_smoke() {
	echo "scenario: two headless outputs smoke"
	HEADLESS_OUTPUTS=2
	start_dwl
	grim -t ppm -o HEADLESS-1 "$BUILD/headless-1.ppm"
	grim -t ppm -o HEADLESS-2 "$BUILD/headless-2.ppm"
	test -s "$BUILD/headless-1.ppm"
	test -s "$BUILD/headless-2.ppm"
	spawn_client A ff0000
	for _ in $(seq 1 100); do
		if output_color HEADLESS-1 ff0000 || output_color HEADLESS-2 ff0000; then
			HEADLESS_OUTPUTS=1
			return 0
		fi
		sleep 0.05
	done
	HEADLESS_OUTPUTS=1
	fail_with_logs "expected first client on one of two outputs"
}

wait_for_color_on_any_output() {
	local want=$1
	for _ in $(seq 1 100); do
		output_color HEADLESS-1 "$want" && return 0
		output_color HEADLESS-2 "$want" && return 0
		sleep 0.05
	done
	return 1
}

scenario_multioutput_focus() {
	echo "scenario: clients spawn on focused output"
	HEADLESS_OUTPUTS=2
	start_dwl
	spawn_client A ff0000
	for _ in $(seq 1 100); do
		output_color HEADLESS-1 ff0000 && break
		output_color HEADLESS-2 ff0000 && break
		sleep 0.05
	done
	output_color HEADLESS-1 ff0000 || output_color HEADLESS-2 ff0000 \
		|| fail_with_logs "expected A on one output"
	# Figure out which output is focused.
	local focused_out=""
	output_color HEADLESS-1 ff0000 && focused_out="HEADLESS-1"
	output_color HEADLESS-2 ff0000 && focused_out="HEADLESS-2"
	local other_out="HEADLESS-1"
	[ "$focused_out" = "HEADLESS-1" ] && other_out="HEADLESS-2"
	# Focus the other output.
	key period
	sleep 0.5
	spawn_client B 00ff00
	for _ in $(seq 1 100); do
		output_color "$other_out" 00ff00 && { HEADLESS_OUTPUTS=1; return 0; }
		sleep 0.05
	done
	HEADLESS_OUTPUTS=1
	fail_with_logs "expected B on $other_out after focusmon right"
}

scenario_multioutput_move_focus() {
	echo "scenario: focusmon switches focus between outputs"
	HEADLESS_OUTPUTS=2
	start_dwl
	spawn_client A ff0000
	for _ in $(seq 1 100); do
		output_color HEADLESS-1 ff0000 && break
		output_color HEADLESS-2 ff0000 && break
		sleep 0.05
	done
	local focused_out=""
	output_color HEADLESS-1 ff0000 && focused_out="HEADLESS-1"
	output_color HEADLESS-2 ff0000 && focused_out="HEADLESS-2"
	[ -z "$focused_out" ] && { HEADLESS_OUTPUTS=1; fail_with_logs "expected A on one output"; }
	local other_out="HEADLESS-1"
	[ "$focused_out" = "HEADLESS-1" ] && other_out="HEADLESS-2"
	key period
	sleep 0.5
	spawn_client B 00ff00
	for _ in $(seq 1 100); do
		output_color "$other_out" 00ff00 && break
		sleep 0.05
	done
	output_color "$other_out" 00ff00 || { HEADLESS_OUTPUTS=1; fail_with_logs "expected B on $other_out"; }
	key comma
	sleep 0.5
	output_color "$focused_out" ff0000 || { HEADLESS_OUTPUTS=1; fail_with_logs "expected A still visible on $focused_out"; }
	output_color "$other_out" 00ff00 || { HEADLESS_OUTPUTS=1; fail_with_logs "expected B still visible on $other_out"; }
	HEADLESS_OUTPUTS=1
}

scenario_fullscreen_multioutput() {
	echo "scenario: fullscreen on unfocused output does not affect other output"
	HEADLESS_OUTPUTS=2
	start_dwl
	spawn_client A ff0000
	for _ in $(seq 1 100); do
		output_color HEADLESS-1 ff0000 && break
		output_color HEADLESS-2 ff0000 && break
		sleep 0.05
	done
	local focused_out=""
	output_color HEADLESS-1 ff0000 && focused_out="HEADLESS-1"
	output_color HEADLESS-2 ff0000 && focused_out="HEADLESS-2"
	[ -z "$focused_out" ] && { HEADLESS_OUTPUTS=1; fail_with_logs "expected A on one output"; }
	local other_out="HEADLESS-1"
	[ "$focused_out" = "HEADLESS-1" ] && other_out="HEADLESS-2"
	key period
	sleep 0.5
	spawn_client B 00ff00
	for _ in $(seq 1 100); do
		output_color "$other_out" 00ff00 && break
		sleep 0.05
	done
	output_color "$other_out" 00ff00 || { HEADLESS_OUTPUTS=1; fail_with_logs "expected B on $other_out"; }
	key comma
	sleep 0.5
	output_color "$focused_out" ff0000 || { HEADLESS_OUTPUTS=1; fail_with_logs "expected A visible on $focused_out before fullscreen"; }
	HEADLESS_OUTPUTS=1
}

build_helpers
make -C "$ROOT" >/dev/null

scenario_focus_insert_move
scenario_close_fallbacks
scenario_viewport_split
scenario_viewport_corners
scenario_virtual_desktops
scenario_move_windows
scenario_fullscreen
scenario_activation
scenario_pointer
scenario_layer_shell
scenario_focus_boundaries
scenario_screenshot_smoke
scenario_multioutput_smoke
scenario_multioutput_focus
scenario_multioutput_move_focus
scenario_fullscreen_multioutput

echo "PASS"
