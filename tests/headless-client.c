#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>
#include <wayland-client.h>

#include "xdg-activation-v1-client-protocol.h"
#include "xdg-shell-client-protocol.h"

struct client {
	struct wl_display *display;
	struct wl_compositor *compositor;
	struct wl_seat *seat;
	struct wl_shm *shm;
	struct xdg_activation_v1 *activation;
	struct xdg_wm_base *wm_base;
	struct wl_surface *surface;
	struct xdg_surface *xdg_surface;
	struct xdg_toplevel *toplevel;
	struct wl_buffer *buffer;
	int configured;
	int running;
	int fullscreen;
	int activate_before_map;
	int activate_after_map;
	int activate_requested_token;
	uint32_t color;
	const char *title;
	const char *appid;
	const char *activate_token;
	const char *token_file;
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
	char name[] = "/dwl-test-client-XXXXXX";
	const char *runtime = getenv("XDG_RUNTIME_DIR");
	char path[512];
	int fd;

	if (!runtime)
		return -1;
	snprintf(path, sizeof(path), "%s/%s", runtime, name + 1);
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
create_buffer(struct client *c, int width, int height, uint32_t color)
{
	int stride = width * 4;
	int size = stride * height;
	int fd = create_shm_file((size_t)size);
	uint32_t *data;
	struct wl_shm_pool *pool;
	struct wl_buffer *buffer;

	if (fd < 0)
		die("create_shm_file");
	data = mmap(NULL, (size_t)size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if (data == MAP_FAILED)
		die("mmap");
	for (int i = 0; i < width * height; i++)
		data[i] = color;
	munmap(data, (size_t)size);
	pool = wl_shm_create_pool(c->shm, fd, size);
	buffer = wl_shm_pool_create_buffer(pool, 0, width, height, stride, WL_SHM_FORMAT_XRGB8888);
	wl_shm_pool_destroy(pool);
	close(fd);
	return buffer;
}

static void
xdg_wm_base_ping(void *data, struct xdg_wm_base *base, uint32_t serial)
{
	xdg_wm_base_pong(base, serial);
}

static const struct xdg_wm_base_listener wm_base_listener = {
	.ping = xdg_wm_base_ping,
};

static void
xdg_surface_configure(void *data, struct xdg_surface *surface, uint32_t serial)
{
	struct client *c = data;
	xdg_surface_ack_configure(surface, serial);
	c->configured = 1;
}

static const struct xdg_surface_listener xdg_surface_listener = {
	.configure = xdg_surface_configure,
};

static void
toplevel_configure(void *data, struct xdg_toplevel *toplevel, int32_t width, int32_t height,
                   struct wl_array *states)
{
}

static void
toplevel_close(void *data, struct xdg_toplevel *toplevel)
{
	struct client *c = data;
	c->running = 0;
}

static void
toplevel_configure_bounds(void *data, struct xdg_toplevel *toplevel, int32_t width,
                          int32_t height)
{
}

static void
toplevel_wm_capabilities(void *data, struct xdg_toplevel *toplevel, struct wl_array *capabilities)
{
}

static const struct xdg_toplevel_listener toplevel_listener = {
	.configure = toplevel_configure,
	.close = toplevel_close,
	.configure_bounds = toplevel_configure_bounds,
	.wm_capabilities = toplevel_wm_capabilities,
};

static void
activation_token_done(void *data, struct xdg_activation_token_v1 *token, const char *token_string)
{
	struct client *c = data;

	if (c->token_file) {
		FILE *file;
		file = fopen(c->token_file, "w");
		if (!file)
			die("fopen token_file");
		fprintf(file, "%s\n", token_string);
		fclose(file);
	}
	if (c->activate_requested_token)
		xdg_activation_v1_activate(c->activation, token_string, c->surface);
	xdg_activation_token_v1_destroy(token);
}

static const struct xdg_activation_token_v1_listener activation_token_listener = {
	.done = activation_token_done,
};

static void
request_activation_token(struct client *c)
{
	struct xdg_activation_token_v1 *token;

	if (!c->activation || (!c->token_file && !c->activate_requested_token))
		return;
	token = xdg_activation_v1_get_activation_token(c->activation);
	xdg_activation_token_v1_add_listener(token, &activation_token_listener, c);
	xdg_activation_token_v1_set_app_id(token, c->appid);
	xdg_activation_token_v1_set_surface(token, c->surface);
	xdg_activation_token_v1_commit(token);
}

static void
activate_self(struct client *c)
{
	if (c->activation && c->activate_token)
		xdg_activation_v1_activate(c->activation, c->activate_token, c->surface);
}

static void
registry_global(void *data, struct wl_registry *registry, uint32_t name, const char *interface,
                uint32_t version)
{
	struct client *c = data;
	if (!strcmp(interface, wl_compositor_interface.name))
		c->compositor = wl_registry_bind(registry, name, &wl_compositor_interface, 4);
	else if (!strcmp(interface, wl_seat_interface.name))
		c->seat = wl_registry_bind(registry, name, &wl_seat_interface, 7);
	else if (!strcmp(interface, wl_shm_interface.name))
		c->shm = wl_registry_bind(registry, name, &wl_shm_interface, 1);
	else if (!strcmp(interface, xdg_activation_v1_interface.name))
		c->activation = wl_registry_bind(registry, name, &xdg_activation_v1_interface, 1);
	else if (!strcmp(interface, xdg_wm_base_interface.name)) {
		c->wm_base = wl_registry_bind(registry, name, &xdg_wm_base_interface, 2);
		xdg_wm_base_add_listener(c->wm_base, &wm_base_listener, c);
	}
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
	fprintf(stderr, "usage: %s --title TITLE --appid APPID [--color RRGGBB] [--fullscreen] "
	                "[--token-file PATH] [--activate-requested-token] "
	                "[--activate-token TOKEN --activate-before-map|--activate-after-map]\n",
	        argv0);
	exit(2);
}

static void
handle_signal(int signo)
{
}

int
main(int argc, char **argv)
{
	struct client c = {.running = 1, .title = "test", .appid = "test", .color = 0xff204060u};
	struct wl_registry *registry;
	struct sigaction sa = {.sa_handler = handle_signal};

	for (int i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "--title") && i + 1 < argc)
			c.title = argv[++i];
		else if (!strcmp(argv[i], "--appid") && i + 1 < argc)
			c.appid = argv[++i];
		else if (!strcmp(argv[i], "--color") && i + 1 < argc)
			c.color = 0xff000000u | (uint32_t)strtoul(argv[++i], NULL, 16);
		else if (!strcmp(argv[i], "--fullscreen"))
			c.fullscreen = 1;
		else if (!strcmp(argv[i], "--token-file") && i + 1 < argc)
			c.token_file = argv[++i];
		else if (!strcmp(argv[i], "--activate-requested-token"))
			c.activate_requested_token = 1;
		else if (!strcmp(argv[i], "--activate-token") && i + 1 < argc)
			c.activate_token = argv[++i];
		else if (!strcmp(argv[i], "--activate-before-map"))
			c.activate_before_map = 1;
		else if (!strcmp(argv[i], "--activate-after-map"))
			c.activate_after_map = 1;
		else
			usage(argv[0]);
	}

	sigaction(SIGTERM, &sa, NULL);
	sigaction(SIGINT, &sa, NULL);
	c.display = wl_display_connect(NULL);
	if (!c.display)
		die("wl_display_connect");
	registry = wl_display_get_registry(c.display);
	wl_registry_add_listener(registry, &registry_listener, &c);
	wl_display_roundtrip(c.display);
	if (!c.compositor || !c.shm || !c.wm_base) {
		fprintf(stderr, "missing required globals\n");
		return 1;
	}

	c.surface = wl_compositor_create_surface(c.compositor);
	c.xdg_surface = xdg_wm_base_get_xdg_surface(c.wm_base, c.surface);
	xdg_surface_add_listener(c.xdg_surface, &xdg_surface_listener, &c);
	c.toplevel = xdg_surface_get_toplevel(c.xdg_surface);
	xdg_toplevel_add_listener(c.toplevel, &toplevel_listener, &c);
	xdg_toplevel_set_title(c.toplevel, c.title);
	xdg_toplevel_set_app_id(c.toplevel, c.appid);
	if (c.fullscreen)
		xdg_toplevel_set_fullscreen(c.toplevel, NULL);
	if (c.activate_before_map)
		activate_self(&c);
	wl_surface_commit(c.surface);

	while (!c.configured && wl_display_dispatch(c.display) != -1)
		;
	c.buffer = create_buffer(&c, 160, 120, c.color);
	wl_surface_attach(c.surface, c.buffer, 0, 0);
	wl_surface_damage_buffer(c.surface, 0, 0, 160, 120);
	wl_surface_commit(c.surface);
	wl_display_flush(c.display);
	if (c.activate_after_map)
		activate_self(&c);
	request_activation_token(&c);
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
	if (c.toplevel)
		xdg_toplevel_destroy(c.toplevel);
	if (c.xdg_surface)
		xdg_surface_destroy(c.xdg_surface);
	if (c.surface)
		wl_surface_destroy(c.surface);
	if (c.wm_base)
		xdg_wm_base_destroy(c.wm_base);
	if (c.activation)
		xdg_activation_v1_destroy(c.activation);
	if (c.shm)
		wl_shm_destroy(c.shm);
	if (c.seat)
		wl_seat_destroy(c.seat);
	if (c.compositor)
		wl_compositor_destroy(c.compositor);
	wl_registry_destroy(registry);
	wl_display_disconnect(c.display);
	return 0;
}
