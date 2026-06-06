/* Taken from https://github.com/djpohly/dwl/issues/466 */
#define COLOR(hex)    { ((hex >> 24) & 0xFF) / 255.0f, \
                        ((hex >> 16) & 0xFF) / 255.0f, \
                        ((hex >> 8) & 0xFF) / 255.0f, \
                        (hex & 0xFF) / 255.0f }
/* appearance */
static const int sloppyfocus               = 1;  /* focus follows mouse */
static const int bypass_surface_visibility = 0;  /* 1 means idle inhibitors will disable idle tracking even if it's surface isn't visible  */
static const unsigned int borderpx         = 1;  /* border pixel of windows */
static const int showbar                   = 1; /* 0 means no bar */
static const int topbar                    = 1; /* 0 means bottom bar */
static const char *fonts[]                 = {"monospace:size=10"};
static const float rootcolor[]             = COLOR(0x000000ff);
/* This conforms to the xdg-protocol. Set the alpha to zero to restore the old behavior */
static const float fullscreen_bg[]         = {0.1f, 0.1f, 0.1f, 1.0f}; /* You can also use glsl colors */
static uint32_t colors[][3]                = {
	/*               fg          bg          border    */
	[SchemeNorm] = { 0xbbbbbbff, 0x222222ff, 0x444444ff },
	[SchemeSel]  = { 0xeeeeeeff, 0x005577ff, 0x005577ff },
	[SchemeUrg]  = { 0,          0,          0x770000ff },
};
static const char col_gray1[]       = "#222222";
static const char col_gray2[]       = "#444444";
static const char col_gray3[]       = "#bbbbbb";
static const char col_gray4[]       = "#eeeeee";
static const char col_cyan[]        = "#005577";

/* virtual desktops */
#define virtual_desktop_count 10
static char *virtual_desktop_names[virtual_desktop_count] = { "0", "1", "2", "3", "4", "5", "6", "7", "8", "9" };

/* logging */
static int log_level = WLR_ERROR;

/* layout(s) */
static const Layout layouts[] = {
	/* symbol     arrange function */
	{ "[S]",      scrolling },
	{ "><>",      NULL },    /* no layout function means floating behavior */
};

/* monitors */
/* (x=-1, y=-1) is reserved as an "autoconfigure" monitor position indicator
 * WARNING: negative values other than (-1, -1) cause problems with Xwayland clients
 * https://gitlab.freedesktop.org/xorg/xserver/-/issues/899
*/
/* NOTE: ALWAYS add a fallback rule, even if you are completely sure it won't be used */
static const MonitorRule monrules[] = {
	/* name       scale layout       rotate/reflect                x    y */
	/* example of a HiDPI laptop monitor:
	{ "eDP-1",    2,    &layouts[0], WL_OUTPUT_TRANSFORM_NORMAL,   -1,  -1 },
	*/
	/* defaults */
	{ NULL,       1,    &layouts[0], WL_OUTPUT_TRANSFORM_NORMAL,   -1,  -1 },
};

/* keyboard */
static const struct xkb_rule_names xkb_rules = {
	/* can specify fields: rules, model, layout, variant, options */
	/* example:
	.options = "ctrl:nocaps",
	*/
	.options = NULL,
};

static const int repeat_rate = 25;
static const int repeat_delay = 600;

/* Trackpad */
static const int tap_to_click = 1;
static const int tap_and_drag = 1;
static const int drag_lock = 1;
static const int natural_scrolling = 0;
static const int disable_while_typing = 1;
static const int left_handed = 0;
static const int middle_button_emulation = 0;
/* You can choose between:
LIBINPUT_CONFIG_SCROLL_NO_SCROLL
LIBINPUT_CONFIG_SCROLL_2FG
LIBINPUT_CONFIG_SCROLL_EDGE
LIBINPUT_CONFIG_SCROLL_ON_BUTTON_DOWN
*/
static const enum libinput_config_scroll_method scroll_method = LIBINPUT_CONFIG_SCROLL_2FG;

/* You can choose between:
LIBINPUT_CONFIG_CLICK_METHOD_NONE
LIBINPUT_CONFIG_CLICK_METHOD_BUTTON_AREAS
LIBINPUT_CONFIG_CLICK_METHOD_CLICKFINGER
*/
static const enum libinput_config_click_method click_method = LIBINPUT_CONFIG_CLICK_METHOD_BUTTON_AREAS;

/* You can choose between:
LIBINPUT_CONFIG_SEND_EVENTS_ENABLED
LIBINPUT_CONFIG_SEND_EVENTS_DISABLED
LIBINPUT_CONFIG_SEND_EVENTS_DISABLED_ON_EXTERNAL_MOUSE
*/
static const uint32_t send_events_mode = LIBINPUT_CONFIG_SEND_EVENTS_ENABLED;

/* You can choose between:
LIBINPUT_CONFIG_ACCEL_PROFILE_FLAT
LIBINPUT_CONFIG_ACCEL_PROFILE_ADAPTIVE
*/
static const enum libinput_config_accel_profile accel_profile = LIBINPUT_CONFIG_ACCEL_PROFILE_ADAPTIVE;
static const double accel_speed = 0.0;

/* You can choose between:
LIBINPUT_CONFIG_TAP_MAP_LRM -- 1/2/3 finger tap maps to left/right/middle
LIBINPUT_CONFIG_TAP_MAP_LMR -- 1/2/3 finger tap maps to left/middle/right
*/
static const enum libinput_config_tap_button_map button_map = LIBINPUT_CONFIG_TAP_MAP_LRM;

/* If you want to use the windows key for MODKEY, use WLR_MODIFIER_LOGO */
//#define MODKEY WLR_MODIFIER_ALT
#define MODKEY WLR_MODIFIER_LOGO
//#define MODKEY WLR_MODIFIER_CTRL

/* commands */
static const char *termcmd[] = { "st", NULL };
static const char *menucmd[] = { "mew-run", "-f", "monospace:size=10", "-nb", col_gray1, "-nf", col_gray3, "-sb", col_cyan, "-sf", col_gray4, NULL };
static const char *lockcmd[] = { "waylock", "-fork-on-lock", NULL };

static const Key keys[] = {
	/* Note that Shift changes certain key codes: c -> C, 2 -> at, etc. */
	/* modifier                  key                 function        argument */
	{ MODKEY,                    XKB_KEY_p,          spawn,          {.v = menucmd} },
	{ MODKEY,                    XKB_KEY_Return,     spawn,          {.v = termcmd} },
	{ MODKEY|WLR_MODIFIER_CTRL,  XKB_KEY_l,          spawn,          {.v = lockcmd} },
	{ MODKEY|WLR_MODIFIER_SHIFT, XKB_KEY_plus,       split,          {.i = +1} },
	{ MODKEY,                    XKB_KEY_equal,      split,          {.i = +1} },
	{ MODKEY,                    XKB_KEY_minus,      split,          {.i = -1} },
	{ MODKEY,                    XKB_KEY_i,          split,          {.i = +1} },
	{ MODKEY,                    XKB_KEY_d,          split,          {.i = -1} },
	{ MODKEY,                    XKB_KEY_Left,       movefocus,      {.i = WLR_DIRECTION_LEFT} },
	{ MODKEY,                    XKB_KEY_Right,      movefocus,      {.i = WLR_DIRECTION_RIGHT} },
	{ MODKEY,                    XKB_KEY_Up,         movefocus,      {.i = WLR_DIRECTION_UP} },
	{ MODKEY,                    XKB_KEY_Down,       movefocus,      {.i = WLR_DIRECTION_DOWN} },
	{ MODKEY|WLR_MODIFIER_SHIFT, XKB_KEY_Left,       movewindow,     {.i = WLR_DIRECTION_LEFT} },
	{ MODKEY|WLR_MODIFIER_SHIFT, XKB_KEY_Right,      movewindow,     {.i = WLR_DIRECTION_RIGHT} },
	{ MODKEY|WLR_MODIFIER_SHIFT, XKB_KEY_Up,         movewindow,     {.i = WLR_DIRECTION_UP} },
	{ MODKEY|WLR_MODIFIER_SHIFT, XKB_KEY_Down,       movewindow,     {.i = WLR_DIRECTION_DOWN} },
	{ MODKEY,                    XKB_KEY_h,          movefocus,      {.i = WLR_DIRECTION_LEFT} },
	{ MODKEY,                    XKB_KEY_l,          movefocus,      {.i = WLR_DIRECTION_RIGHT} },
	{ MODKEY,                    XKB_KEY_comma,      focusmon,       {.i = WLR_DIRECTION_LEFT} },
	{ MODKEY,                    XKB_KEY_period,     focusmon,       {.i = WLR_DIRECTION_RIGHT} },
	{ MODKEY|WLR_MODIFIER_SHIFT, XKB_KEY_Return,     zoom,           {0} },
	{ MODKEY|WLR_MODIFIER_SHIFT, XKB_KEY_C,          killclient,     {0} },
	{ MODKEY,                    XKB_KEY_e,          togglefullscreen, {0} },
        { MODKEY,                    XKB_KEY_0,          switch_virtual_desktop, { .i = 0} },
        { MODKEY,                    XKB_KEY_1,          switch_virtual_desktop, { .i = 1} },
        { MODKEY,                    XKB_KEY_2,          switch_virtual_desktop, { .i = 2} },
        { MODKEY,                    XKB_KEY_3,          switch_virtual_desktop, { .i = 3} },
        { MODKEY,                    XKB_KEY_4,          switch_virtual_desktop, { .i = 4} },
        { MODKEY,                    XKB_KEY_5,          switch_virtual_desktop, { .i = 5} },
        { MODKEY,                    XKB_KEY_6,          switch_virtual_desktop, { .i = 6} },
        { MODKEY,                    XKB_KEY_7,          switch_virtual_desktop, { .i = 7} },
        { MODKEY,                    XKB_KEY_8,          switch_virtual_desktop, { .i = 8} },
        { MODKEY,                    XKB_KEY_9,          switch_virtual_desktop, { .i = 9} },
	{ MODKEY|WLR_MODIFIER_SHIFT, XKB_KEY_Q,          quit,           {0} },

	/* Ctrl-Alt-Backspace and Ctrl-Alt-Fx used to be handled by X server */
	{ WLR_MODIFIER_CTRL|WLR_MODIFIER_ALT,XKB_KEY_Terminate_Server, quit, {0} },
	/* Ctrl-Alt-Fx is used to switch to another VT, if you don't know what a VT is
	 * do not remove them.
	 */
#define CHVT(n) { WLR_MODIFIER_CTRL|WLR_MODIFIER_ALT,XKB_KEY_XF86Switch_VT_##n, chvt, {.ui = (n)} }
	CHVT(1), CHVT(2), CHVT(3), CHVT(4), CHVT(5), CHVT(6),
	CHVT(7), CHVT(8), CHVT(9), CHVT(10), CHVT(11), CHVT(12),
};

static const Button buttons[] = {
	{ ClkTagBar,   0,      BTN_LEFT,   switch_virtual_desktop, {0} },
	{ ClkLtSymbol, 0,      BTN_LEFT,   setlayout,      {.v = &layouts[0]} },
	{ ClkLtSymbol, 0,      BTN_RIGHT,  setlayout,      {.v = &layouts[2]} },
	{ ClkTitle,    0,      BTN_MIDDLE, zoom,           {0} },
	{ ClkStatus,   0,      BTN_MIDDLE, spawn,          {.v = termcmd} },
	{ ClkClient,   MODKEY, BTN_LEFT,   moveresize,     {.ui = CurMove} },
	{ ClkClient,   MODKEY, BTN_RIGHT,  moveresize,     {.ui = CurResize} },
};
/* vim: set tabstop=8 softtabstop=0 noexpandtab shiftwidth=8 textwidth=100: */
