#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#include <wayland-client.h>

#include "wlr-layer-shell-unstable-v1-client-protocol.h"

struct client {
	struct wl_display *display;
	struct wl_compositor *compositor;
	struct wl_shm *shm;
	struct zwlr_layer_shell_v1 *layer_shell;
	struct wl_surface *surface;
	struct zwlr_layer_surface_v1 *layer_surface;
	struct wl_buffer *buffer;
	int configured;
	int running;
	uint32_t color;
	uint32_t layer;
	uint32_t keyboard;
	uint32_t width;
	uint32_t height;
};

static void
die(const char *msg)
{
	perror(msg);
	exit(1);
}

static int
create_shm_file(size_t size)
{
	const char *runtime = getenv("XDG_RUNTIME_DIR");
	char path[512];
	int fd;

	if (!runtime)
		return -1;
	snprintf(path, sizeof(path), "%s/dwl-layer-client-XXXXXX", runtime);
	fd = mkstemp(path);
	if (fd < 0)
		return -1;
	unlink(path);
	if (ftruncate(fd, (off_t)size) < 0) {
		close(fd);
		return -1;
	}
	return fd;
}

static struct wl_buffer *
create_buffer(struct client *c)
{
	int stride = (int)c->width * 4;
	int size = stride * (int)c->height;
	int fd = create_shm_file((size_t)size);
	uint32_t *data;
	struct wl_shm_pool *pool;
	struct wl_buffer *buffer;

	if (fd < 0)
		die("create_shm_file");
	data = mmap(NULL, (size_t)size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if (data == MAP_FAILED)
		die("mmap");
	for (uint32_t i = 0; i < c->width * c->height; i++)
		data[i] = c->color;
	munmap(data, (size_t)size);
	pool = wl_shm_create_pool(c->shm, fd, size);
	buffer = wl_shm_pool_create_buffer(pool, 0, (int)c->width, (int)c->height, stride,
	                                  WL_SHM_FORMAT_XRGB8888);
	wl_shm_pool_destroy(pool);
	close(fd);
	return buffer;
}

static void
layer_surface_configure(void *data, struct zwlr_layer_surface_v1 *surface, uint32_t serial,
                        uint32_t width, uint32_t height)
{
	struct client *c = data;
	zwlr_layer_surface_v1_ack_configure(surface, serial);
	c->configured = 1;
}

static void
layer_surface_closed(void *data, struct zwlr_layer_surface_v1 *surface)
{
	struct client *c = data;
	c->running = 0;
}

static const struct zwlr_layer_surface_v1_listener layer_surface_listener = {
	.configure = layer_surface_configure,
	.closed = layer_surface_closed,
};

static void
registry_global(void *data, struct wl_registry *registry, uint32_t name, const char *interface,
                uint32_t version)
{
	struct client *c = data;
	if (!strcmp(interface, wl_compositor_interface.name))
		c->compositor = wl_registry_bind(registry, name, &wl_compositor_interface, 4);
	else if (!strcmp(interface, wl_shm_interface.name))
		c->shm = wl_registry_bind(registry, name, &wl_shm_interface, 1);
	else if (!strcmp(interface, zwlr_layer_shell_v1_interface.name))
		c->layer_shell = wl_registry_bind(registry, name, &zwlr_layer_shell_v1_interface, 3);
}

static void
registry_remove(void *data, struct wl_registry *registry, uint32_t name)
{
}

static const struct wl_registry_listener registry_listener = {
	.global = registry_global,
	.global_remove = registry_remove,
};

static void
usage(const char *argv0)
{
	fprintf(stderr, "usage: %s [--color RRGGBB] [--exclusive] [--top|--overlay]\n", argv0);
	exit(2);
}

int
main(int argc, char **argv)
{
	struct client c = {.running = 1,
	                   .color = 0xff808080u,
	                   .layer = ZWLR_LAYER_SHELL_V1_LAYER_TOP,
	                   .keyboard = ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_NONE,
	                   .width = 1280,
	                   .height = 720};
	struct wl_registry *registry;

	for (int i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "--color") && i + 1 < argc)
			c.color = 0xff000000u | (uint32_t)strtoul(argv[++i], NULL, 16);
		else if (!strcmp(argv[i], "--exclusive"))
			c.keyboard = ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_EXCLUSIVE;
		else if (!strcmp(argv[i], "--top"))
			c.layer = ZWLR_LAYER_SHELL_V1_LAYER_TOP;
		else if (!strcmp(argv[i], "--overlay"))
			c.layer = ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY;
		else
			usage(argv[0]);
	}

	c.display = wl_display_connect(NULL);
	if (!c.display)
		die("wl_display_connect");
	registry = wl_display_get_registry(c.display);
	wl_registry_add_listener(registry, &registry_listener, &c);
	wl_display_roundtrip(c.display);
	if (!c.compositor || !c.shm || !c.layer_shell) {
		fprintf(stderr, "missing required globals\n");
		return 1;
	}

	c.surface = wl_compositor_create_surface(c.compositor);
	c.layer_surface = zwlr_layer_shell_v1_get_layer_surface(c.layer_shell, c.surface, NULL,
	                                                       c.layer, "dwl-test-layer");
	zwlr_layer_surface_v1_add_listener(c.layer_surface, &layer_surface_listener, &c);
	zwlr_layer_surface_v1_set_size(c.layer_surface, c.width, c.height);
	zwlr_layer_surface_v1_set_anchor(c.layer_surface, ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP |
	                                                   ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM |
	                                                   ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT |
	                                                   ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT);
	zwlr_layer_surface_v1_set_exclusive_zone(c.layer_surface, -1);
	zwlr_layer_surface_v1_set_keyboard_interactivity(c.layer_surface, c.keyboard);
	wl_surface_commit(c.surface);
	while (!c.configured && wl_display_dispatch(c.display) != -1)
		;
	c.buffer = create_buffer(&c);
	wl_surface_attach(c.surface, c.buffer, 0, 0);
	wl_surface_damage_buffer(c.surface, 0, 0, (int)c.width, (int)c.height);
	wl_surface_commit(c.surface);
	wl_display_flush(c.display);

	while (c.running) {
		if (wl_display_dispatch(c.display) == -1) {
			if (errno == EINTR)
				break;
			return 1;
		}
	}

	if (c.buffer)
		wl_buffer_destroy(c.buffer);
	if (c.layer_surface)
		zwlr_layer_surface_v1_destroy(c.layer_surface);
	if (c.surface)
		wl_surface_destroy(c.surface);
	if (c.layer_shell)
		zwlr_layer_shell_v1_destroy(c.layer_shell);
	if (c.shm)
		wl_shm_destroy(c.shm);
	if (c.compositor)
		wl_compositor_destroy(c.compositor);
	wl_registry_destroy(registry);
	wl_display_disconnect(c.display);
	return 0;
}
