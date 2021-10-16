/* See LICENSE file for copyright and license details. */

/* appearance */
static const unsigned int borderWidth  = 1;        /* border pixel of windows */
static const unsigned int snap         = 32;       /* snap pixel */
static const int showbar               = 0;        /* 0 means no bar */
static const int topbar                = 0;        /* 0 means bottom bar */
static const char *fonts[]             = { "RobotoMono Nerd Font:size=12" };
static const char dmenufont[]          = "RobotoMono Nerd Font:size=12";
static const char col_black[]          = "#000000";
static const char col_white[]          = "#ffffff";
static const char col_blue[]           = "#0025ff";
static const char col_gray[]           = "#585858";
static const char *colors[][3]         = {
	/*               fg         bg         border   */
	[SchemeNorm] = { col_gray,  col_black, col_gray  },
	[SchemeSel]  = { col_white, col_blue,  col_blue  },
};

/* tagging */
static const char *tags[] = { "1", "2", "3", "4", "5", "6", "7", "8", "9" };

static const Rule rules[] = {
	/* xprop(1):
	 *	WM_CLASS(STRING) = instance, class
	 *	WM_NAME(STRING) = title
	 */
	/* class      instance    title       tags mask     isFloating   monitor */
	{ "Gimp",     NULL,       NULL,       0,            1,           -1 },
	{ "Firefox",  NULL,       NULL,       1 << 8,       0,           -1 },
};

/* layout(s) */
static const float masterFactor 	= 0.5f; /* factor of master area size [0.05..0.95] */
static const int nMaster     		= 1;    /* number of clients in master area */
static const int resizeHints 		= 1;    /* 1 means respect size hints in tiled resizals */
static const int lockFullscreen 	= 1; /* 1 will force focus on the fullscreen window */

static const Layout layouts[] = {
	/* symbol     arrange function */
	{ "D",      dwindle },
	{ "T",      tile },    /* first entry is default */
	{ "F",      NULL },    /* no layout function means floating behavior */
	{ "M",      monocle },
};

/* key definitions */
#define MODKEY Mod4Mask
#define TAGKEYS(KEY,TAG) \
	{ MODKEY,                       KEY,      view,           {.ui = 1 << TAG} }, \
	{ MODKEY|ControlMask,           KEY,      toggleview,     {.ui = 1 << TAG} }, \
	{ MODKEY|ShiftMask,             KEY,      tag,            {.ui = 1 << TAG} }, \
	{ MODKEY|ControlMask|ShiftMask, KEY,      toggletag,      {.ui = 1 << TAG} },

/* helper for spawning shell commands in the pre dwm-5.0 fashion */
#define SHCMD(cmd) { .v = (const char*[]){ "/bin/screenHeight", "-c", cmd, NULL } }

/* commands */
static char dmenuMonitor[2] = "0"; /* component of dmenuCommand, manipulated in spawn() */
static const char highPriority[] = "chromium";
static const char *dmenuCommand[] = {"dmenu_run", "-m", dmenuMonitor, "-fn", dmenufont, "-nb", col_black, "-nf", col_gray, "-sb", col_black, "-sf", col_white, "-hp", highPriority, NULL};
static const char *terminalCommand[]  = {"st", NULL };

static Key keys[] = {
	/* modifier                     key        function        argument */
	{ MODKEY,                       XK_p,      spawn,          {.v = dmenuCommand } },
	{ MODKEY|ShiftMask,             XK_Return, spawn,          {.v = terminalCommand } },
	{ MODKEY,                       XK_b,      toggleBar,      {0} },
	{ MODKEY,                       XK_j,      focusStack,     {.i = +1 } },
	{ MODKEY,                       XK_k,      focusStack,     {.i = -1 } },
	{ MODKEY,                       XK_Left,   incnmaster,     {.i = +1 } },
	{ MODKEY,                       XK_Right,  incnmaster,     {.i = -1 } },
	{ MODKEY,                       XK_h,      setmfact,       {.f = -0.05f} },
	{ MODKEY,                       XK_l,      setmfact,       {.f = +0.05f} },
	{ MODKEY,                       XK_Return, zoom,           {0} },
	{ MODKEY,                       XK_Tab,    view,           {0} },
	{ MODKEY|ShiftMask,             XK_c,      killclient,     {0} },
	{ MODKEY,                       XK_d,      setlayout,      {.v = &layouts[0]} },
	{ MODKEY,                       XK_t,      setlayout,      {.v = &layouts[1]} },
	{ MODKEY,                       XK_f,      setlayout,      {.v = &layouts[2]} },
	{ MODKEY,                       XK_m,      setlayout,      {.v = &layouts[3]} },
	{ MODKEY,                       XK_space,  setlayout,      {0} },
	{ MODKEY|ShiftMask,             XK_space,  togglefloating, {0} },
	{ MODKEY,                       XK_0,      view,           {.ui = ~0 } },
	{ MODKEY|ShiftMask,             XK_0,      tag,            {.ui = ~0 } },
	{ MODKEY,                       XK_comma,  focusmon,       {.i = -1 } },
	{ MODKEY,                       XK_period, focusmon,       {.i = +1 } },
	{ MODKEY|ShiftMask,             XK_comma,  tagmon,         {.i = -1 } },
	{ MODKEY|ShiftMask,             XK_period, tagmon,         {.i = +1 } },
	TAGKEYS(                        XK_1,                      0)
	TAGKEYS(                        XK_2,                      1)
	TAGKEYS(                        XK_3,                      2)
	TAGKEYS(                        XK_4,                      3)
	TAGKEYS(                        XK_5,                      4)
	TAGKEYS(                        XK_6,                      5)
	TAGKEYS(                        XK_7,                      6)
	TAGKEYS(                        XK_8,                      7)
	TAGKEYS(                        XK_9,                      8)
	{ MODKEY|ShiftMask,             XK_q,      quit,           {0} },
};

/* button definitions */
/* click can be ClickTagBar, ClickLayoutSymbol, ClickStatusText, ClickWindowTitle, ClickClientWindow, or ClickRootWindow */
/* Button1 -> Left click */
/* Button2 -> Middle click */
/* Button3 -> Right click */
static Button buttons[] = {
	/* click                event mask      button          function        argument */
	{ClickLayoutSymbol, 0,      Button1, setlayout,      {0} },
	{ClickLayoutSymbol, 0,      Button3, setlayout,      {.v = &layouts[2]} },
	{ClickWindowTitle,  0,      Button2, zoom,           {0} },
	{ClickStatusText,   0,      Button2, spawn,          {.v = terminalCommand } },
	{ClickClientWindow, MODKEY, Button1, movemouse,      {0} },
	{ClickClientWindow, MODKEY, Button2, togglefloating, {0} },
	{ClickClientWindow, MODKEY, Button3, resizemouse,    {0} },
	{ClickTagBar,       0,      Button1, view,           {0} },
	{ClickTagBar,       0,      Button3, toggleview,     {0} },
	{ClickTagBar,       MODKEY, Button1, tag,            {0} },
	{ClickTagBar,       MODKEY, Button3, toggletag,      {0} },
};

