#define _POSIX_C_SOURCE 200809L

#include <linux/input-event-codes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <wayland-client.h>

#include "wlr-virtual-pointer-unstable-v1-client-protocol.h"

struct state {
	struct wl_display *display;
	struct wl_seat *seat;
	struct zwlr_virtual_pointer_manager_v1 *manager;
	struct zwlr_virtual_pointer_v1 *pointer;
};

static void
registry_global(void *data, struct wl_registry *registry, uint32_t name, const char *interface,
                uint32_t version)
{
	struct state *s = data;
	if (!strcmp(interface, zwlr_virtual_pointer_manager_v1_interface.name))
		s->manager = wl_registry_bind(registry, name,
		                              &zwlr_virtual_pointer_manager_v1_interface, 2);
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

int
main(int argc, char **argv)
{
	struct state s = {0};
	struct wl_registry *registry;
	uint32_t x, y;
	struct timespec ts = {.tv_nsec = 50000000};

	if (argc != 3) {
		fprintf(stderr, "usage: %s x y\n", argv[0]);
		return 2;
	}
	x = (uint32_t)strtoul(argv[1], NULL, 10);
	y = (uint32_t)strtoul(argv[2], NULL, 10);

	s.display = wl_display_connect(NULL);
	if (!s.display) {
		perror("wl_display_connect");
		return 1;
	}
	registry = wl_display_get_registry(s.display);
	wl_registry_add_listener(registry, &registry_listener, &s);
	wl_display_roundtrip(s.display);
	if (!s.manager || !s.seat) {
		fprintf(stderr, "missing virtual pointer manager or seat\n");
		return 1;
	}
	s.pointer = zwlr_virtual_pointer_manager_v1_create_virtual_pointer(s.manager, s.seat);
	zwlr_virtual_pointer_v1_motion_absolute(s.pointer, 0, x, y, 1280, 720);
	zwlr_virtual_pointer_v1_frame(s.pointer);
	zwlr_virtual_pointer_v1_button(s.pointer, 1, BTN_LEFT, WL_POINTER_BUTTON_STATE_PRESSED);
	zwlr_virtual_pointer_v1_frame(s.pointer);
	zwlr_virtual_pointer_v1_button(s.pointer, 2, BTN_LEFT, WL_POINTER_BUTTON_STATE_RELEASED);
	zwlr_virtual_pointer_v1_frame(s.pointer);
	wl_display_flush(s.display);
	nanosleep(&ts, NULL);

	zwlr_virtual_pointer_v1_destroy(s.pointer);
	zwlr_virtual_pointer_manager_v1_destroy(s.manager);
	wl_seat_destroy(s.seat);
	wl_registry_destroy(registry);
	wl_display_disconnect(s.display);
	return 0;
}
