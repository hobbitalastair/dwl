#define _POSIX_C_SOURCE 200809L

#include <fcntl.h>
#include <linux/input-event-codes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>
#include <wayland-client.h>
#include <xkbcommon/xkbcommon.h>

#include "virtual-keyboard-unstable-v1-client-protocol.h"

#define WLR_MODIFIER_SHIFT 1u
#define WLR_MODIFIER_LOGO 64u

struct state {
	struct wl_display *display;
	struct wl_seat *seat;
	struct zwp_virtual_keyboard_manager_v1 *manager;
	struct zwp_virtual_keyboard_v1 *keyboard;
};

static int
create_tmp_file(size_t size)
{
	char tmpl[] = "/tmp/dwl-virtual-keymap-XXXXXX";
	int fd = mkstemp(tmpl);
	if (fd < 0)
		return -1;
	unlink(tmpl);
	if (ftruncate(fd, (off_t)size) < 0) {
		close(fd);
		return -1;
	}
	return fd;
}

static void
send_keymap(struct state *s)
{
	struct xkb_context *ctx = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
	struct xkb_rule_names names = {0};
	struct xkb_keymap *keymap = xkb_keymap_new_from_names(ctx, &names, XKB_KEYMAP_COMPILE_NO_FLAGS);
	char *str = xkb_keymap_get_as_string(keymap, XKB_KEYMAP_FORMAT_TEXT_V1);
	size_t len = strlen(str) + 1;
	int fd = create_tmp_file(len);
	void *data;

	if (fd < 0) {
		perror("create_tmp_file");
		exit(1);
	}
	data = mmap(NULL, len, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if (data == MAP_FAILED) {
		perror("mmap");
		exit(1);
	}
	memcpy(data, str, len);
	munmap(data, len);
	zwp_virtual_keyboard_v1_keymap(s->keyboard, WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1, fd, (uint32_t)len);
	close(fd);
	free(str);
	xkb_keymap_unref(keymap);
	xkb_context_unref(ctx);
}

static void
registry_global(void *data, struct wl_registry *registry, uint32_t name, const char *interface,
                uint32_t version)
{
	struct state *s = data;
	if (!strcmp(interface, zwp_virtual_keyboard_manager_v1_interface.name))
		s->manager = wl_registry_bind(registry, name,
		                              &zwp_virtual_keyboard_manager_v1_interface, 1);
	else if (!strcmp(interface, wl_seat_interface.name))
		s->seat = wl_registry_bind(registry, name, &wl_seat_interface, 7);
}

static void
registry_remove(void *data, struct wl_registry *registry, uint32_t name)
{
}

static const struct wl_registry_listener registry_listener = {
	.global = registry_global,
	.global_remove = registry_remove,
};

static int
keycode_for(const char *name)
{
	if (!strcmp(name, "left"))
		return KEY_LEFT;
	if (!strcmp(name, "right"))
		return KEY_RIGHT;
	if (!strcmp(name, "up"))
		return KEY_UP;
	if (!strcmp(name, "down"))
		return KEY_DOWN;
	if (!strcmp(name, "0"))
		return KEY_0;
	if (!strcmp(name, "1"))
		return KEY_1;
	if (!strcmp(name, "2"))
		return KEY_2;
	if (!strcmp(name, "c"))
		return KEY_C;
	if (!strcmp(name, "e"))
		return KEY_E;
	if (!strcmp(name, "i"))
		return KEY_I;
	if (!strcmp(name, "d"))
		return KEY_D;
	if (!strcmp(name, "comma"))
		return KEY_COMMA;
	if (!strcmp(name, "period"))
		return KEY_DOT;
	fprintf(stderr, "unknown key: %s\n", name);
	exit(2);
}

int
main(int argc, char **argv)
{
	struct state s = {0};
	struct wl_registry *registry;
	uint32_t mods = 0;
	int logo = 1;
	int key;

	if (argc < 2) {
		fprintf(stderr, "usage: %s [--shift] key\n", argv[0]);
		return 2;
	}
	for (int i = 1; i < argc - 1; i++) {
		if (!strcmp(argv[i], "--shift"))
			mods |= WLR_MODIFIER_SHIFT;
		else if (!strcmp(argv[i], "--no-logo"))
			logo = 0;
		else {
			fprintf(stderr, "unknown option: %s\n", argv[i]);
			return 2;
		}
	}
	key = keycode_for(argv[argc - 1]);

	s.display = wl_display_connect(NULL);
	if (!s.display) {
		perror("wl_display_connect");
		return 1;
	}
	registry = wl_display_get_registry(s.display);
	wl_registry_add_listener(registry, &registry_listener, &s);
	wl_display_roundtrip(s.display);
	if (!s.manager || !s.seat) {
		fprintf(stderr, "missing virtual keyboard manager or seat\n");
		return 1;
	}
	s.keyboard = zwp_virtual_keyboard_manager_v1_create_virtual_keyboard(s.manager, s.seat);
	send_keymap(&s);
	wl_display_roundtrip(s.display);

	if (logo)
		zwp_virtual_keyboard_v1_key(s.keyboard, 1, KEY_LEFTMETA, WL_KEYBOARD_KEY_STATE_PRESSED);
	if (mods & WLR_MODIFIER_SHIFT)
		zwp_virtual_keyboard_v1_key(s.keyboard, 2, KEY_LEFTSHIFT, WL_KEYBOARD_KEY_STATE_PRESSED);
	zwp_virtual_keyboard_v1_modifiers(s.keyboard, mods | (logo ? WLR_MODIFIER_LOGO : 0), 0, 0, 0);
	zwp_virtual_keyboard_v1_key(s.keyboard, 3, key, WL_KEYBOARD_KEY_STATE_PRESSED);
	zwp_virtual_keyboard_v1_key(s.keyboard, 4, key, WL_KEYBOARD_KEY_STATE_RELEASED);
	if (mods & WLR_MODIFIER_SHIFT)
		zwp_virtual_keyboard_v1_key(s.keyboard, 5, KEY_LEFTSHIFT, WL_KEYBOARD_KEY_STATE_RELEASED);
	if (logo)
		zwp_virtual_keyboard_v1_key(s.keyboard, 6, KEY_LEFTMETA, WL_KEYBOARD_KEY_STATE_RELEASED);
	zwp_virtual_keyboard_v1_modifiers(s.keyboard, 0, 0, 0, 0);
	wl_display_flush(s.display);
	{
		struct timespec ts = {.tv_nsec = 50000000};
		nanosleep(&ts, NULL);
	}

	zwp_virtual_keyboard_v1_destroy(s.keyboard);
	zwp_virtual_keyboard_manager_v1_destroy(s.manager);
	wl_seat_destroy(s.seat);
	wl_registry_destroy(registry);
	wl_display_disconnect(s.display);
	return 0;
}
