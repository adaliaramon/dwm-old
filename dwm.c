/* See LICENSE file for copyright and license details.
 *
 * dynamic window manager is designed like any other X client as well. It is
 * driven through handling X events. In contrast to other X clients, a window
 * manager selects for SubstructureRedirectMask on the root window, to receive
 * events about window (dis-)appearance. Only one X connection at a time is
 * allowed to select for this event mask.
 *
 * The event handlers of dwm are organized in an array which is accessed
 * whenever a new event has been fetched. This allows event dispatching
 * in O(1) time.
 *
 * Each child of the root window is called a client, except windows which have
 * set the override_redirect flag. Clients are organized in a linked client
 * list on each monitor, the focus history is remembered through a stack list
 * on each monitor. Each client contains a bit array to indicate the tags of a
 * client.
 *
 * Keys and tagging rules are organized as arrays and defined in config.h.
 *
 * To understand everything else, start reading main().
 */
#include <locale.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <X11/cursorfont.h>
#include <X11/keysym.h>
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xproto.h>
#include <X11/Xutil.h>
#ifdef XINERAMA
#include <X11/extensions/Xinerama.h>
#endif /* XINERAMA */
#include <X11/Xft/Xft.h>

#include "drw.h"
#include "util.h"

/* macros */
#define BUTTONMASK              (ButtonPressMask|ButtonReleaseMask)
#define CLEANMASK(mask)         (mask & ~(numlockmask|LockMask) & (ShiftMask|ControlMask|Mod1Mask|Mod2Mask|Mod3Mask|Mod4Mask|Mod5Mask))
#define INTERSECT(x,y,w,h,m)    (MAX(0, MIN((x)+(w),(m)->windowX+(m)->windowWidth) - MAX((x),(m)->windowX)) \
                               * MAX(0, MIN((y)+(h),(m)->windowY+(m)->windowHeight) - MAX((y),(m)->windowY)))
#define ISVISIBLEONTAG(C, T)    ((C->tags & T))
#define ISVISIBLE(C)            ISVISIBLEONTAG(C, C->monitor->tagSet[C->monitor->selectedTags])
#define LENGTH(X)               (sizeof X / sizeof X[0])
#define MOUSEMASK               (BUTTONMASK|PointerMotionMask)
#define WIDTH(X)                ((X)->w + 2 * (X)->borderWidth)
#define HEIGHT(X)               ((X)->h + 2 * (X)->borderWidth)
#define TAGMASK                 ((1 << LENGTH(tags)) - 1)
#define TEXTW(X)                (drw_fontset_getwidth(draw, (X)) + leftRightPad)

/* enums */
enum { CurNormal, CurResize, CurMove, CurLast }; /* cursor */
enum { SchemeNorm, SchemeSel }; /* color schemes */
enum { NetSupported, NetWMName, NetWMState, NetWMCheck,
       NetWMFullscreen, NetActiveWindow, NetWMWindowType,
       NetWMWindowTypeDialog, NetClientList, NetLast }; /* EWMH atoms */
enum { WMProtocols, WMDelete, WMState, WMTakeFocus, WMLast }; /* default atoms */
enum { ClickTagBar, ClickLayoutSymbol, ClickStatusText, ClickWindowTitle,
       ClickClientWindow, ClickRootWindow, ClkLast }; /* clicks */

typedef union {
	int i;
	unsigned int ui;
	float f;
	const void *v;
} Argument;

typedef struct {
	unsigned int click;
	unsigned int mask;
	unsigned int button;
	void (*function)(const Argument *argument);
	const Argument argument;
} Button;

typedef struct Monitor Monitor;
typedef struct Client { // Any regular window (not a bar window, I believe)
	char name[256];
	float mina, maxa;
	int x, y, w, h;
	int oldx, oldy, oldw, oldh;
	int basew, baseh, incw, inch, maxw, maxh, minw, minh;
	int borderWidth, oldBorderWidth;
	unsigned int tags;
	int isFixed, isFloating, isUrgent, neverFocus, oldState, isFullscreen;
	struct Client *next; // Next client (Super + j)
	struct Client *selectionNext; // Next client in the order that they were selected
	Monitor *monitor;
	Window window;
} Client;

typedef struct {
	unsigned int modifier;
	KeySym keySymbol;
	void (*function)(const Argument *);
	const Argument argument;
} Key;

typedef struct {
	const char *symbol;
	void (*arrange)(Monitor *);
} Layout;

struct Monitor {
	char layoutSymbol[16];
	float masterFactor;
	int nMaster;
	int num;
	int by;               /* bar geometry */
	int monitorX, monitorY, monitorWidth, monitorHeight;   /* screen size */
	int windowX, windowY, windowWidth, windowHeight;   /* window area  */
	unsigned int selectedTags;
	unsigned int selectedLayout;
	unsigned int tagSet[2];
	int showBar;
	int topBar;
	Client *clients;
	Client *selectedClient;
	Client *stack;
	Monitor *next;
	Window barWindow;
	const Layout *layouts[2];
};

typedef struct {
	const char *class;
	const char *instance;
	const char *title;
	unsigned int tags;
	int isfloating;
	int monitor;
} Rule;

/* function declarations */
static void applyrules(Client *c);
static int applysizehints(Client *c, int *x, int *y, int *w, int *h, int interact);
static void arrange(Monitor *m);
static void arrangemon(Monitor *m);
static void attach(Client *c);
static void attachBelow(Client *c);
static void attachStack(Client *c);
static void buttonPress(XEvent *event);
static void checkOtherWindowManager(void);
static void cleanup(void);
static void cleanupmon(Monitor *mon);
static void clientmessage(XEvent *e);
static void configure(Client *c);
static void configurenotify(XEvent *e);
static void configurerequest(XEvent *e);
static Monitor *createMonitor(void);
static void destroynotify(XEvent *e);
static void detach(Client *c);
static void detachStack(Client *c);
static Monitor *dirtomon(int dir);
static void drawBar(Monitor *monitor);
static void drawBars(void);
static void enternotify(XEvent *e);
static void expose(XEvent *e);
static void focus(Client *client);
static void focusin(XEvent *e);
static void focusmon(const Argument *arg);
static void focusStack(const Argument *argument);
static Atom getatomprop(Client *c, Atom prop);
static int getRootPointer(int *x, int *y);
static long getstate(Window w);
static int gettextprop(Window w, Atom atom, char *text, unsigned int size);
static void grabButtons(Client *c, int focused);
static void grabkeys(void);
static void incnmaster(const Argument *arg);
static void keyPress(XEvent *event);
static void killclient(const Argument *arg);
static void manage(Window w, XWindowAttributes *wa);
static void mappingnotify(XEvent *e);
static void maprequest(XEvent *e);
static void monocle(Monitor *m);
static void motionNotify(XEvent *e);
static void movemouse(const Argument *arg);
static Client *nexttagged(Client *c);
static Client *nexttiled(Client *c);
static void pop(Client *);
static void propertynotify(XEvent *e);
static void quit(const Argument *arg);
static Monitor *rectangleToMonitor(int x, int y, int w, int h);
static void resize(Client *c, int x, int y, int w, int h, int interact);
static void resizeclient(Client *c, int x, int y, int w, int h);
static void resizemouse(const Argument *arg);
static void restack(Monitor *m);
static void run(void);
static void scan(void);
static int sendevent(Client *c, Atom proto);
static void sendmon(Client *c, Monitor *m);
static void setclientstate(Client *c, long state);
static void setFocus(Client *c);
static void setfullscreen(Client *c, int fullscreen);
static void setlayout(const Argument *arg);
static void setmfact(const Argument *arg);
static void setup(void);
static void seturgent(Client *c, int urg);
static void showhide(Client *c);
static void sigchld(int unused);
static void spawn(const Argument *argument);
static void tag(const Argument *arg);
static void tagmon(const Argument *arg);
static void tile(Monitor *);
static void dwindle(Monitor *);
static void toggleBar(const Argument *argument);
static void togglefloating(const Argument *arg);
static void toggletag(const Argument *arg);
static void toggleview(const Argument *arg);
static void unfocus(Client *c, int setfocus);
static void unmanage(Client *c, int destroyed);
static void unmapnotify(XEvent *e);
static void updatebarpos(Monitor *m);
static void updatebars(void);
static void updateclientlist(void);
static int updateGeometry(void);
static void updatenumlockmask(void);
static void updatesizehints(Client *c);
static void updatestatus(void);
static void updatetitle(Client *c);
static void updatewindowtype(Client *c);
static void updatewmhints(Client *c);
static void view(const Argument *arg);
static Client *windowToClient(Window window);
static Monitor *windowToMonitor(Window window);
static int xerror(Display *dpy, XErrorEvent *ee);
static int xerrordummy(Display *dpy, XErrorEvent *ee);
static int xerrorstart(Display *dpy, XErrorEvent *ee);
static void zoom(const Argument *arg);

/* variables */
static const char broken[] = "broken";
static char statusText[256]; // Bottom left text, dwm-version by default. It is set with xsetroot
static int screen;
static int screenWidth, screenHeight; // X display screen geometry width, height
static int barHeight, barLayoutWidth = 0; // Bar geometry, blw -> barLayoutWidth/barLeftWidth (?) to be determined
static int leftRightPad; // Sum of left and right padding for text
static int (*xerrorxlib)(Display *, XErrorEvent *);
static unsigned int numlockmask = 0;
static void (*handler[LASTEvent]) (XEvent *) = {
	[ButtonPress] = buttonPress, // Mouse button click handler
	[ClientMessage] = clientmessage,
	[ConfigureRequest] = configurerequest,
	[ConfigureNotify] = configurenotify,
	[DestroyNotify] = destroynotify,
	[EnterNotify] = enternotify,
	[Expose] = expose,
	[FocusIn] = focusin,
	[KeyPress] = keyPress, // Keyboard handler
	[MappingNotify] = mappingnotify,
	[MapRequest] = maprequest,
	[MotionNotify] = motionNotify,
	[PropertyNotify] = propertynotify,
	[UnmapNotify] = unmapnotify
};
static Atom wmatom[WMLast], netatom[NetLast];
static int running = 1;
static Cur *cursor[CurLast];
static Color **scheme;
static Display *display;
static Draw *draw;
static Monitor *monitors, *selectedMonitor; // Monitors really points to the first monitor in a linked list
static Window root, wmcheckwin; // Root is the main window, parent to all the other windows

/* configuration, allows nested code to access above variables */
#include "config.h"

/* compile-time check if all tags fit into an unsigned int bit array. */
struct NumTags { char limitexceeded[LENGTH(tags) > 31 ? -1 : 1]; };

/* function implementations */
void
applyrules(Client *c)
{
	const char *class, *instance;
	unsigned int i;
	const Rule *r;
	Monitor *m;
	XClassHint ch = { NULL, NULL };

	/* rule matching */
	c->isFloating = 0;
	c->tags = 0;
	XGetClassHint(display, c->window, &ch);
	class    = ch.res_class ? ch.res_class : broken;
	instance = ch.res_name  ? ch.res_name  : broken;

	for (i = 0; i < LENGTH(rules); i++) {
		r = &rules[i];
		if ((!r->title || strstr(c->name, r->title))
		&& (!r->class || strstr(class, r->class))
		&& (!r->instance || strstr(instance, r->instance)))
		{
			c->isFloating = r->isfloating;
			c->tags |= r->tags;
			for (m = monitors; m && m->num != r->monitor; m = m->next);
			if (m)
				c->monitor = m;
		}
	}
	if (ch.res_class)
		XFree(ch.res_class);
	if (ch.res_name)
		XFree(ch.res_name);
	c->tags = c->tags & TAGMASK ? c->tags & TAGMASK : c->monitor->tagSet[c->monitor->selectedTags];
}

int
applysizehints(Client *c, int *x, int *y, int *w, int *h, int interact)
{
	int baseismin;
	Monitor *m = c->monitor;

	/* set minimum possible */
	*w = MAX(1, *w);
	*h = MAX(1, *h);
	if (interact) {
		if (*x > screenWidth)
			*x = screenWidth - WIDTH(c);
		if (*y > screenHeight)
			*y = screenHeight - HEIGHT(c);
		if (*x + *w + 2 * c->borderWidth < 0)
			*x = 0;
		if (*y + *h + 2 * c->borderWidth < 0)
			*y = 0;
	} else {
		if (*x >= m->windowX + m->windowWidth)
			*x = m->windowX + m->windowWidth - WIDTH(c);
		if (*y >= m->windowY + m->windowHeight)
			*y = m->windowY + m->windowHeight - HEIGHT(c);
		if (*x + *w + 2 * c->borderWidth <= m->windowX)
			*x = m->windowX;
		if (*y + *h + 2 * c->borderWidth <= m->windowY)
			*y = m->windowY;
	}
	if (*h < barHeight)
		*h = barHeight;
	if (*w < barHeight)
		*w = barHeight;
	if (resizeHints || c->isFloating || !c->monitor->layouts[c->monitor->selectedLayout]->arrange) {
		/* see last two sentences in ICCCM 4.1.2.3 */
		baseismin = c->basew == c->minw && c->baseh == c->minh;
		if (!baseismin) { /* temporarily remove base dimensions */
			*w -= c->basew;
			*h -= c->baseh;
		}
		/* adjust for aspect limits */
		if (c->mina > 0 && c->maxa > 0) {
			if (c->maxa < (float)*w / *h)
				*w = *h * c->maxa + 0.5;
			else if (c->mina < (float)*h / *w)
				*h = *w * c->mina + 0.5;
		}
		if (baseismin) { /* increment calculation requires this */
			*w -= c->basew;
			*h -= c->baseh;
		}
		/* adjust for increment value */
		if (c->incw)
			*w -= *w % c->incw;
		if (c->inch)
			*h -= *h % c->inch;
		/* restore base dimensions */
		*w = MAX(*w + c->basew, c->minw);
		*h = MAX(*h + c->baseh, c->minh);
		if (c->maxw)
			*w = MIN(*w, c->maxw);
		if (c->maxh)
			*h = MIN(*h, c->maxh);
	}
	return *x != c->x || *y != c->y || *w != c->w || *h != c->h;
}

void
arrange(Monitor *m)
{
	if (m)
		showhide(m->stack);
	else for (m = monitors; m; m = m->next)
		showhide(m->stack);
	if (m) {
		arrangemon(m);
		restack(m);
	} else for (m = monitors; m; m = m->next)
		arrangemon(m);
}

void
arrangemon(Monitor *m)
{
	strncpy(m->layoutSymbol, m->layouts[m->selectedLayout]->symbol, sizeof m->layoutSymbol);
	if (m->layouts[m->selectedLayout]->arrange)
		m->layouts[m->selectedLayout]->arrange(m);
}

void
attach(Client *c)
{
	c->next = c->monitor->clients;
	c->monitor->clients = c;
}
void
attachBelow(Client *c)
{
	//If there is nothing on the monitor or the selected client is floating, attach as normal
	if(c->monitor->selectedClient == NULL || c->monitor->selectedClient->isFloating) {
        Client *at = nexttagged(c);
        if(!at) {
            attach(c);
            return;
            }
        c->next = at->next;
        at->next = c;
		return;
	}

	//Set the new client's next property to the same as the currently selected clients next
	c->next = c->monitor->selectedClient->next;
	//Set the currently selected clients next property to the new client
	c->monitor->selectedClient->next = c;

}

void attachStack(Client *c) {
	c->selectionNext = c->monitor->stack;
	c->monitor->stack = c;
}

void buttonPress(XEvent *event) { // Mouse button press handler, does not seem to trigger when clicking on a Window...
	unsigned int i, x, click;
	Argument argument = {0};
	Client *client;
	Monitor *monitor;
	XButtonPressedEvent *buttonPressedEvent = &event->xbutton;

	click = ClickRootWindow;
	/* Focus monitor if necessary */
	if ((monitor = windowToMonitor(buttonPressedEvent->window)) && monitor != selectedMonitor) {
		unfocus(selectedMonitor->selectedClient, 1);
        selectedMonitor = monitor;
		focus(NULL);
	}
	if (buttonPressedEvent->window == selectedMonitor->barWindow) { // If the bar window was clicked
		i = x = 0;
        /* Keep increasing the x position  along with the tag index (i) until we either
         * surpass the x position of the click or we go through all the tags
         * Just a way to check which tag we clicked, if any, stored as an index (i)
         */
        do {
            x += TEXTW(tags[i]);
        } while (buttonPressedEvent->x >= x && ++i < LENGTH(tags));
		if (i < LENGTH(tags)) {
			click = ClickTagBar; // Set the click type
			argument.ui = 1 << i; // Set the unsigned integer argument part to some value depending on i (tag mask (?))
		} else if (buttonPressedEvent->x < x + barLayoutWidth)
			click = ClickLayoutSymbol; // If the layout button was clicked, set the click type
		else if (buttonPressedEvent->x > selectedMonitor->windowWidth - (int)TEXTW(statusText))
			click = ClickStatusText; // Check if the status text was clicked
		else
			click = ClickWindowTitle;
	} else if ((client = windowToClient(buttonPressedEvent->window))) {
		focus(client);
		restack(selectedMonitor);
		XAllowEvents(display, ReplayPointer, CurrentTime);
		click = ClickClientWindow;
	}
    for (i = 0; i < LENGTH(buttons); i++) {
        if (click == buttons[i].click && buttons[i].function && buttons[i].button == buttonPressedEvent->button
            && CLEANMASK(buttons[i].mask) == CLEANMASK(buttonPressedEvent->state)) {
            buttons[i].function(click == ClickTagBar && buttons[i].argument.i == 0 ? &argument : &buttons[i].argument);
        }
    }
}

void checkOtherWindowManager(void) {
	xerrorxlib = XSetErrorHandler(xerrorstart);
	/* this causes an error if some other window manager is running */
	XSelectInput(display, DefaultRootWindow(display), SubstructureRedirectMask);
	XSync(display, False);
	XSetErrorHandler(xerror);
	XSync(display, False);
}

void
cleanup(void)
{
	Argument a = {.ui = ~0};
	Layout foo = { "", NULL };
	Monitor *m;
	size_t i;

	view(&a);
    selectedMonitor->layouts[selectedMonitor->selectedLayout] = &foo;
	for (m = monitors; m; m = m->next)
		while (m->stack)
			unmanage(m->stack, 0);
	XUngrabKey(display, AnyKey, AnyModifier, root);
	while (monitors)
		cleanupmon(monitors);
	for (i = 0; i < CurLast; i++)
		drw_cur_free(draw, cursor[i]);
	for (i = 0; i < LENGTH(colors); i++)
		free(scheme[i]);
	XDestroyWindow(display, wmcheckwin);
	drw_free(draw);
	XSync(display, False);
	XSetInputFocus(display, PointerRoot, RevertToPointerRoot, CurrentTime);
	XDeleteProperty(display, root, netatom[NetActiveWindow]);
}

void
cleanupmon(Monitor *mon)
{
	Monitor *m;

	if (mon == monitors)
        monitors = monitors->next;
	else {
		for (m = monitors; m && m->next != mon; m = m->next);
		m->next = mon->next;
	}
	XUnmapWindow(display, mon->barWindow);
	XDestroyWindow(display, mon->barWindow);
	free(mon);
}

void
clientmessage(XEvent *e)
{
	XClientMessageEvent *cme = &e->xclient;
	Client *c = windowToClient(cme->window);

	if (!c)
		return;
	if (cme->message_type == netatom[NetWMState]) {
		if (cme->data.l[1] == netatom[NetWMFullscreen]
		|| cme->data.l[2] == netatom[NetWMFullscreen])
			setfullscreen(c, (cme->data.l[0] == 1 /* _NET_WM_STATE_ADD    */
				|| (cme->data.l[0] == 2 /* _NET_WM_STATE_TOGGLE */ && !c->isFullscreen)));
	} else if (cme->message_type == netatom[NetActiveWindow]) {
		if (c != selectedMonitor->selectedClient && !c->isUrgent)
			seturgent(c, 1);
	}
}

void
configure(Client *c)
{
	XConfigureEvent ce;

	ce.type = ConfigureNotify;
	ce.display = display;
	ce.event = c->window;
	ce.window = c->window;
	ce.x = c->x;
	ce.y = c->y;
	ce.width = c->w;
	ce.height = c->h;
	ce.border_width = c->borderWidth;
	ce.above = None;
	ce.override_redirect = False;
	XSendEvent(display, c->window, False, StructureNotifyMask, (XEvent *)&ce);
}

void
configurenotify(XEvent *e)
{
	Monitor *m;
	Client *c;
	XConfigureEvent *ev = &e->xconfigure;
	int dirty;

	/* TODO: updateGeometry handling sucks, needs to be simplified */
	if (ev->window == root) {
		dirty = (screenWidth != ev->width || screenHeight != ev->height);
        screenWidth = ev->width;
        screenHeight = ev->height;
		if (updateGeometry() || dirty) {
			drw_resize(draw, screenWidth, barHeight);
			updatebars();
			for (m = monitors; m; m = m->next) {
				for (c = m->clients; c; c = c->next)
					if (c->isFullscreen)
						resizeclient(c, m->monitorX, m->monitorY, m->monitorWidth, m->monitorHeight);
				XMoveResizeWindow(display, m->barWindow, m->windowX, m->by, m->windowWidth, barHeight);
			}
			focus(NULL);
			arrange(NULL);
		}
	}
}

void
configurerequest(XEvent *e)
{
	Client *c;
	Monitor *m;
	XConfigureRequestEvent *ev = &e->xconfigurerequest;
	XWindowChanges wc;

	if ((c = windowToClient(ev->window))) {
		if (ev->value_mask & CWBorderWidth)
			c->borderWidth = ev->border_width;
		else if (c->isFloating || !selectedMonitor->layouts[selectedMonitor->selectedLayout]->arrange) {
			m = c->monitor;
			if (ev->value_mask & CWX) {
				c->oldx = c->x;
				c->x = m->monitorX + ev->x;
			}
			if (ev->value_mask & CWY) {
				c->oldy = c->y;
				c->y = m->monitorY + ev->y;
			}
			if (ev->value_mask & CWWidth) {
				c->oldw = c->w;
				c->w = ev->width;
			}
			if (ev->value_mask & CWHeight) {
				c->oldh = c->h;
				c->h = ev->height;
			}
			if ((c->x + c->w) > m->monitorX + m->monitorWidth && c->isFloating)
				c->x = m->monitorX + (m->monitorWidth / 2 - WIDTH(c) / 2); /* center in x direction */
			if ((c->y + c->h) > m->monitorY + m->monitorHeight && c->isFloating)
				c->y = m->monitorY + (m->monitorHeight / 2 - HEIGHT(c) / 2); /* center in y direction */
			if ((ev->value_mask & (CWX|CWY)) && !(ev->value_mask & (CWWidth|CWHeight)))
				configure(c);
			if (ISVISIBLE(c))
				XMoveResizeWindow(display, c->window, c->x, c->y, c->w, c->h);
		} else
			configure(c);
	} else {
		wc.x = ev->x;
		wc.y = ev->y;
		wc.width = ev->width;
		wc.height = ev->height;
		wc.border_width = ev->border_width;
		wc.sibling = ev->above;
		wc.stack_mode = ev->detail;
		XConfigureWindow(display, ev->window, ev->value_mask, &wc);
	}
	XSync(display, False);
}

Monitor * createMonitor(void) {
    Monitor *monitor = ecalloc(1, sizeof(Monitor));
    monitor->tagSet[0] = monitor->tagSet[1] = 1;
    monitor->masterFactor = masterFactor;
    monitor->nMaster = nMaster;
    monitor->showBar = showbar;
    monitor->topBar = topbar;
    monitor->layouts[0] = &layouts[0];
    monitor->layouts[1] = &layouts[1 % LENGTH(layouts)];
	strncpy(monitor->layoutSymbol, layouts[0].symbol, sizeof monitor->layoutSymbol);
	return monitor;
}

void
destroynotify(XEvent *e)
{
	Client *c;
	XDestroyWindowEvent *ev = &e->xdestroywindow;

	if ((c = windowToClient(ev->window)))
		unmanage(c, 1);
}

void
detach(Client *c)
{
	Client **tc;

	for (tc = &c->monitor->clients; *tc && *tc != c; tc = &(*tc)->next);
	*tc = c->next;
}

void detachStack(Client *c) {
	Client **tc, *t;

	for (tc = &c->monitor->stack; *tc && *tc != c; tc = &(*tc)->selectionNext);
	*tc = c->selectionNext;

	if (c == c->monitor->selectedClient) {
		for (t = c->monitor->stack; t && !ISVISIBLE(t); t = t->selectionNext);
		c->monitor->selectedClient = t;
	}
}

Monitor *
dirtomon(int dir)
{
	Monitor *m = NULL;

	if (dir > 0) {
		if (!(m = selectedMonitor->next))
			m = monitors;
	} else if (selectedMonitor == monitors)
		for (m = monitors; m->next; m = m->next);
	else
		for (m = monitors; m->next != selectedMonitor; m = m->next);
	return m;
}

void drawBar(Monitor *monitor) {
	int x, w, textWidth = 0, mw, ew = 0;
	const unsigned int boxs = draw->fonts->height / 9;
	const unsigned int boxw = draw->fonts->height / 6 + 2;
	unsigned int i, occ = 0, urg = 0, n = 0;
	Client *c;

	/* Draw status first, so it can be overdrawn by tags later */
	if (monitor == selectedMonitor) { /* Status is only drawn on selected monitor */
        drawSetColorScheme(draw, scheme[SchemeNorm]);
        textWidth = TEXTW(statusText) - leftRightPad + 2; /* 2px right padding */
		drw_text(draw, monitor->windowWidth - textWidth, 0, textWidth, barHeight, 0, statusText, 0);
	}

	for (c = monitor->clients; c; c = c->next) {
		if (ISVISIBLE(c))
			n++;
		occ |= c->tags;
		if (c->isUrgent)
			urg |= c->tags;
	}
	x = 0;
	for (i = 0; i < LENGTH(tags); i++) {
		w = TEXTW(tags[i]);
        drawSetColorScheme(draw, scheme[monitor->tagSet[monitor->selectedTags] & 1 << i ? SchemeSel : SchemeNorm]);
		drw_text(draw, x, 0, w, barHeight, leftRightPad / 2, tags[i], urg & 1 << i);
		if (occ & 1 << i)
			drw_rect(draw, x + boxs, boxs, boxw, boxw,
                     monitor == selectedMonitor && selectedMonitor->selectedClient && selectedMonitor->selectedClient->tags & 1 << i,
				urg & 1 << i);
		x += w;
	}
    w = barLayoutWidth = TEXTW(monitor->layoutSymbol);
    drawSetColorScheme(draw, scheme[SchemeNorm]);
	x = drw_text(draw, x, 0, w, barHeight, leftRightPad / 2, monitor->layoutSymbol, 0);

	if ((w = monitor->windowWidth - textWidth - x) > barHeight) {
		if (n > 0) {
            textWidth = TEXTW(monitor->selectedClient->name) + leftRightPad;
			mw = (textWidth >= w || n == 1) ? 0 : (w - textWidth) / (n - 1);

			i = 0;
			for (c = monitor->clients; c; c = c->next) {
				if (!ISVISIBLE(c) || c == monitor->selectedClient)
					continue;
                textWidth = TEXTW(c->name);
				if(textWidth < mw)
					ew += (mw - textWidth);
				else
					i++;
			}
			if (i > 0)
				mw += ew / i;

			for (c = monitor->clients; c; c = c->next) {
				if (!ISVISIBLE(c))
					continue;
                textWidth = MIN(monitor->selectedClient == c ? w : mw, TEXTW(c->name));

                drawSetColorScheme(draw, scheme[monitor->selectedClient == c ? SchemeSel : SchemeNorm]);
				if (textWidth > 0) /* trap special handling of 0 in drw_text */
					drw_text(draw, x, 0, textWidth, barHeight, leftRightPad / 2, c->name, 0);
				if (c->isFloating)
					drw_rect(draw, x + boxs, boxs, boxw, boxw, c->isFixed, 0);
				x += textWidth;
				w -= textWidth;
			}
		}
        drawSetColorScheme(draw, scheme[SchemeNorm]);
		drw_rect(draw, x, 0, w, barHeight, 1, 1);
	}
	drw_map(draw, monitor->barWindow, 0, 0, monitor->windowWidth, barHeight);
}

void drawBars(void) {
	for (Monitor *monitor = monitors; monitor; monitor = monitor->next)
        drawBar(monitor);
}

void
enternotify(XEvent *e)
{
	Client *c;
	Monitor *m;
	XCrossingEvent *ev = &e->xcrossing;

	if ((ev->mode != NotifyNormal || ev->detail == NotifyInferior) && ev->window != root)
		return;
	c = windowToClient(ev->window);
	m = c ? c->monitor : windowToMonitor(ev->window);
	if (m != selectedMonitor) {
		unfocus(selectedMonitor->selectedClient, 1);
        selectedMonitor = m;
	} else if (!c || c == selectedMonitor->selectedClient)
		return;
	focus(c);
}

void
expose(XEvent *e)
{
	Monitor *m;
	XExposeEvent *ev = &e->xexpose;

	if (ev->count == 0 && (m = windowToMonitor(ev->window)))
        drawBar(m);
}

void focus(Client *client) {
    /* If no client or an invisible client was passed, set client to the selection-next visible client */
    if (!client || !ISVISIBLE(client)) {
        for (client = selectedMonitor->stack; client && !ISVISIBLE(client); client = client->selectionNext);
    }
    /* Remove focus from the currently selected client, if necessary */
    if (selectedMonitor->selectedClient && selectedMonitor->selectedClient != client) {
        unfocus(selectedMonitor->selectedClient, 0);
    }
	if (client) {
        /* Update the currently selected monitor, if necessary */
        if (client->monitor != selectedMonitor) { 
            selectedMonitor = client->monitor; 
        }
        if (client->isUrgent) { 
            seturgent(client, 0); 
        }
        detachStack(client);
        attachStack(client);
        grabButtons(client, 1);
		XSetWindowBorder(display, client->window, scheme[SchemeSel][ColBorder].pixel);
        setFocus(client);
	} else {
		XSetInputFocus(display, root, RevertToPointerRoot, CurrentTime);
		XDeleteProperty(display, root, netatom[NetActiveWindow]);
	}
    selectedMonitor->selectedClient = client;
    drawBars();
}

/* there are some broken focus acquiring clients needing extra handling */
void
focusin(XEvent *e)
{
	XFocusChangeEvent *ev = &e->xfocus;

	if (selectedMonitor->selectedClient && ev->window != selectedMonitor->selectedClient->window)
        setFocus(selectedMonitor->selectedClient);
}

void
focusmon(const Argument *arg)
{
	Monitor *m;

	if (!monitors->next)
		return;
	if ((m = dirtomon(arg->i)) == selectedMonitor)
		return;
	unfocus(selectedMonitor->selectedClient, 0);
    selectedMonitor = m;
	focus(NULL);
}

/* Set the focus on the next/previous client, depending on whether argument->i is positive or not */
void focusStack(const Argument *argument) {
	Client *client = NULL, *i;

    if (!selectedMonitor->selectedClient) { // If the selected monitor does not contain any selected client
        return;
    }
    if (selectedMonitor->selectedClient->isFullscreen && lockFullscreen) { // If the selected client is on fullscreen
        return;
    }
	if (argument->i > 0) {
        /* Find the next visible client in the stack, starting from the currently selected one */
		for (client = selectedMonitor->selectedClient->next; client && !ISVISIBLE(client); client = client->next);
		if (!client) // If there is no client found (selectedClient may not have a next (?))
            /* Find the first visible client in the monitor */
			for (client = selectedMonitor->clients; client && !ISVISIBLE(client); client = client->next);
	} else {
        /* Iterate through the current monitor's (visible) clients until the next client is the currently selected client */
        for (i = selectedMonitor->clients; i != selectedMonitor->selectedClient; i = i->next) {
            if (ISVISIBLE(i)) {
                client = i;
            }
        }
        if (!client) {
            for (; i; i = i->next) {
                if (ISVISIBLE(i)) {
                    client = i;
                }
            }
        }
	}
	if (client) {
		focus(client);
		restack(selectedMonitor);
	}
}

Atom
getatomprop(Client *c, Atom prop)
{
	int di;
	unsigned long dl;
	unsigned char *p = NULL;
	Atom da, atom = None;

	if (XGetWindowProperty(display, c->window, prop, 0L, sizeof atom, False, XA_ATOM,
                           &da, &di, &dl, &dl, &p) == Success && p) {
		atom = *(Atom *)p;
		XFree(p);
	}
	return atom;
}

int
getRootPointer(int *x, int *y)
{
	int di;
	unsigned int dui;
	Window dummy;

	return XQueryPointer(display, root, &dummy, &dummy, x, y, &di, &di, &dui);
}

long
getstate(Window w)
{
	int format;
	long result = -1;
	unsigned char *p = NULL;
	unsigned long n, extra;
	Atom real;

	if (XGetWindowProperty(display, w, wmatom[WMState], 0L, 2L, False, wmatom[WMState],
                           &real, &format, &n, &extra, (unsigned char **)&p) != Success)
		return -1;
	if (n != 0)
		result = *p;
	XFree(p);
	return result;
}

int
gettextprop(Window w, Atom atom, char *text, unsigned int size)
{
	char **list = NULL;
	int n;
	XTextProperty name;

	if (!text || size == 0)
		return 0;
	text[0] = '\0';
	if (!XGetTextProperty(display, w, &name, atom) || !name.nitems)
		return 0;
	if (name.encoding == XA_STRING)
		strncpy(text, (char *)name.value, size - 1);
	else {
		if (XmbTextPropertyToTextList(display, &name, &list, &n) >= Success && n > 0 && *list) {
			strncpy(text, *list, size - 1);
			XFreeStringList(list);
		}
	}
	text[size - 1] = '\0';
	XFree(name.value);
	return 1;
}

void grabButtons(Client *c, int focused) {
	updatenumlockmask();
	{
		unsigned int i, j;
		unsigned int modifiers[] = { 0, LockMask, numlockmask, numlockmask|LockMask };
		XUngrabButton(display, AnyButton, AnyModifier, c->window);
		if (!focused)
			XGrabButton(display, AnyButton, AnyModifier, c->window, False,
                        BUTTONMASK, GrabModeSync, GrabModeSync, None, None);
		for (i = 0; i < LENGTH(buttons); i++)
			if (buttons[i].click == ClickClientWindow)
				for (j = 0; j < LENGTH(modifiers); j++)
					XGrabButton(display, buttons[i].button,
						buttons[i].mask | modifiers[j],
                                c->window, False, BUTTONMASK,
                                GrabModeAsync, GrabModeSync, None, None);
	}
}

void
grabkeys(void)
{
	updatenumlockmask();
	{
		unsigned int i, j;
		unsigned int modifiers[] = { 0, LockMask, numlockmask, numlockmask|LockMask };
		KeyCode code;

		XUngrabKey(display, AnyKey, AnyModifier, root);
		for (i = 0; i < LENGTH(keys); i++)
			if ((code = XKeysymToKeycode(display, keys[i].keySymbol)))
				for (j = 0; j < LENGTH(modifiers); j++)
					XGrabKey(display, code, keys[i].modifier | modifiers[j], root,
                             True, GrabModeAsync, GrabModeAsync);
	}
}

void
incnmaster(const Argument *arg)
{
    selectedMonitor->nMaster = MAX(selectedMonitor->nMaster + arg->i, 0);
	arrange(selectedMonitor);
}

#ifdef XINERAMA
static int
isuniquegeom(XineramaScreenInfo *unique, size_t n, XineramaScreenInfo *info)
{
	while (n--)
		if (unique[n].x_org == info->x_org && unique[n].y_org == info->y_org
		&& unique[n].width == info->width && unique[n].height == info->height)
			return 0;
	return 1;
}
#endif /* XINERAMA */

void keyPress(XEvent *event) {
	XKeyEvent *keyEvent = &event->xkey; // Get the key-press event
    KeySym keySymbol = XKeycodeToKeysym(display, (KeyCode) keyEvent->keycode, 0); // Get the keycode of the keypress event
    for (unsigned int i = 0; i < LENGTH(keys); i++) { // Iterate through the Keys defined in config.h
        if (keySymbol == keys[i].keySymbol // If the key symbol matches de key symbol of any Key
            && CLEANMASK(keys[i].modifier) == CLEANMASK(keyEvent->state) // The modifier matches too
            && keys[i].function) { // The Key's function is proper
            keys[i].function(&(keys[i].argument)); // Run the Key's function with the Key's argument
        }
    }
}

void
killclient(const Argument *arg)
{
	if (!selectedMonitor->selectedClient)
		return;
	if (!sendevent(selectedMonitor->selectedClient, wmatom[WMDelete])) {
		XGrabServer(display);
		XSetErrorHandler(xerrordummy);
		XSetCloseDownMode(display, DestroyAll);
		XKillClient(display, selectedMonitor->selectedClient->window);
		XSync(display, False);
		XSetErrorHandler(xerror);
		XUngrabServer(display);
	}
}

void
manage(Window w, XWindowAttributes *wa)
{
	Client *c, *t = NULL;
	Window trans = None;
	XWindowChanges wc;

	c = ecalloc(1, sizeof(Client));
	c->window = w;
	/* geometry */
	c->x = c->oldx = wa->x;
	c->y = c->oldy = wa->y;
	c->w = c->oldw = wa->width;
	c->h = c->oldh = wa->height;
	c->oldBorderWidth = wa->border_width;

	updatetitle(c);
	if (XGetTransientForHint(display, w, &trans) && (t = windowToClient(trans))) {
		c->monitor = t->monitor;
		c->tags = t->tags;
	} else {
		c->monitor = selectedMonitor;
		applyrules(c);
	}

	if (c->x + WIDTH(c) > c->monitor->monitorX + c->monitor->monitorWidth)
		c->x = c->monitor->monitorX + c->monitor->monitorWidth - WIDTH(c);
	if (c->y + HEIGHT(c) > c->monitor->monitorY + c->monitor->monitorHeight)
		c->y = c->monitor->monitorY + c->monitor->monitorHeight - HEIGHT(c);
	c->x = MAX(c->x, c->monitor->monitorX);
	/* only fix client y-offset, if the client center might cover the bar */
	c->y = MAX(c->y, ((c->monitor->by == c->monitor->monitorY) && (c->x + (c->w / 2) >= c->monitor->windowX)
                      && (c->x + (c->w / 2) < c->monitor->windowX + c->monitor->windowWidth)) ? barHeight : c->monitor->monitorY);
	c->borderWidth = borderpx;

	wc.border_width = c->borderWidth;
	XConfigureWindow(display, w, CWBorderWidth, &wc);
	XSetWindowBorder(display, w, scheme[SchemeNorm][ColBorder].pixel);
	configure(c); /* propagates border_width, if size doesn't change */
	updatewindowtype(c);
	updatesizehints(c);
	updatewmhints(c);
	XSelectInput(display, w, EnterWindowMask | FocusChangeMask | PropertyChangeMask | StructureNotifyMask);
    grabButtons(c, 0);
	if (!c->isFloating)
        c->isFloating = c->oldState = trans != None || c->isFixed;
	if (c->isFloating)
		XRaiseWindow(display, c->window);
	attachBelow(c);
    attachStack(c);
	XChangeProperty(display, root, netatom[NetClientList], XA_WINDOW, 32, PropModeAppend,
                    (unsigned char *) &(c->window), 1);
	XMoveResizeWindow(display, c->window, c->x + 2 * screenWidth, c->y, c->w, c->h); /* some windows require this */
	setclientstate(c, NormalState);
	if (c->monitor == selectedMonitor)
		unfocus(selectedMonitor->selectedClient, 0);
	c->monitor->selectedClient = c;
	arrange(c->monitor);
	XMapWindow(display, c->window);
	focus(NULL);
}

void
mappingnotify(XEvent *e)
{
	XMappingEvent *ev = &e->xmapping;

	XRefreshKeyboardMapping(ev);
	if (ev->request == MappingKeyboard)
		grabkeys();
}

void
maprequest(XEvent *e)
{
	static XWindowAttributes wa;
	XMapRequestEvent *ev = &e->xmaprequest;

	if (!XGetWindowAttributes(display, ev->window, &wa))
		return;
	if (wa.override_redirect)
		return;
	if (!windowToClient(ev->window))
		manage(ev->window, &wa);
}

void
monocle(Monitor *m)
{
	unsigned int n = 0;
	Client *c;

	for (c = m->clients; c; c = c->next)
		if (ISVISIBLE(c))
			n++;
	if (n > 0) /* override layout symbol */
		snprintf(m->layoutSymbol, sizeof m->layoutSymbol, "[%d]", n);
	for (c = nexttiled(m->clients); c; c = nexttiled(c->next))
		resize(c, m->windowX, m->windowY, m->windowWidth - 2 * c->borderWidth, m->windowHeight - 2 * c->borderWidth, 0);
}

void motionNotify(XEvent *e) { // Movement of the mouse
	static Monitor *mon = NULL;
	Monitor *m;
	XMotionEvent *ev = &e->xmotion;

	if (ev->window != root)
		return;
	if ((m = rectangleToMonitor(ev->x_root, ev->y_root, 1, 1)) != mon && mon) {
		unfocus(selectedMonitor->selectedClient, 1);
        selectedMonitor = m;
		focus(NULL);
	}
	mon = m;
}

void
movemouse(const Argument *arg)
{
	int x, y, ocx, ocy, nx, ny;
	Client *c;
	Monitor *m;
	XEvent ev;
	Time lasttime = 0;

	if (!(c = selectedMonitor->selectedClient))
		return;
	if (c->isFullscreen) /* no support moving fullscreen windows by mouse */
		return;
	restack(selectedMonitor);
	ocx = c->x;
	ocy = c->y;
	if (XGrabPointer(display, root, False, MOUSEMASK, GrabModeAsync, GrabModeAsync,
                     None, cursor[CurMove]->cursor, CurrentTime) != GrabSuccess)
		return;
	if (!getRootPointer(&x, &y))
		return;
	do {
		XMaskEvent(display, MOUSEMASK | ExposureMask | SubstructureRedirectMask, &ev);
		switch(ev.type) {
		case ConfigureRequest:
		case Expose:
		case MapRequest:
			handler[ev.type](&ev);
			break;
		case MotionNotify:
			if ((ev.xmotion.time - lasttime) <= (1000 / 60))
				continue;
			lasttime = ev.xmotion.time;

			nx = ocx + (ev.xmotion.x - x);
			ny = ocy + (ev.xmotion.y - y);
			if (abs(selectedMonitor->windowX - nx) < snap)
				nx = selectedMonitor->windowX;
			else if (abs((selectedMonitor->windowX + selectedMonitor->windowWidth) - (nx + WIDTH(c))) < snap)
				nx = selectedMonitor->windowX + selectedMonitor->windowWidth - WIDTH(c);
			if (abs(selectedMonitor->windowY - ny) < snap)
				ny = selectedMonitor->windowY;
			else if (abs((selectedMonitor->windowY + selectedMonitor->windowHeight) - (ny + HEIGHT(c))) < snap)
				ny = selectedMonitor->windowY + selectedMonitor->windowHeight - HEIGHT(c);
			if (!c->isFloating && selectedMonitor->layouts[selectedMonitor->selectedLayout]->arrange
                && (abs(nx - c->x) > snap || abs(ny - c->y) > snap))
				togglefloating(NULL);
			if (!selectedMonitor->layouts[selectedMonitor->selectedLayout]->arrange || c->isFloating)
				resize(c, nx, ny, c->w, c->h, 1);
			break;
		}
	} while (ev.type != ButtonRelease);
	XUngrabPointer(display, CurrentTime);
	if ((m = rectangleToMonitor(c->x, c->y, c->w, c->h)) != selectedMonitor) {
		sendmon(c, m);
        selectedMonitor = m;
		focus(NULL);
	}
}

 Client *
nexttagged(Client *c) {
	Client *walked = c->monitor->clients;
	for(;
		walked && (walked->isFloating || !ISVISIBLEONTAG(walked, c->tags));
		walked = walked->next
	);
	return walked;
}

Client *
nexttiled(Client *c)
{
	for (; c && (c->isFloating || !ISVISIBLE(c)); c = c->next);
	return c;
}

void
pop(Client *c)
{
	detach(c);
	attach(c);
	focus(c);
	arrange(c->monitor);
}

void
propertynotify(XEvent *e)
{
	Client *c;
	Window trans;
	XPropertyEvent *ev = &e->xproperty;

	if ((ev->window == root) && (ev->atom == XA_WM_NAME))
		updatestatus();
	else if (ev->state == PropertyDelete)
		return; /* ignore */
	else if ((c = windowToClient(ev->window))) {
		switch(ev->atom) {
		default: break;
		case XA_WM_TRANSIENT_FOR:
			if (!c->isFloating && (XGetTransientForHint(display, c->window, &trans)) &&
                (c->isFloating = (windowToClient(trans)) != NULL))
				arrange(c->monitor);
			break;
		case XA_WM_NORMAL_HINTS:
			updatesizehints(c);
			break;
		case XA_WM_HINTS:
			updatewmhints(c);
                drawBars();
			break;
		}
		if (ev->atom == XA_WM_NAME || ev->atom == netatom[NetWMName]) {
			updatetitle(c);
			if (c == c->monitor->selectedClient)
                drawBar(c->monitor);
		}
		if (ev->atom == netatom[NetWMWindowType])
			updatewindowtype(c);
	}
}

void
quit(const Argument *arg)
{
	running = 0;
}

Monitor *
rectangleToMonitor(int x, int y, int w, int h)
{
	Monitor *m, *r = selectedMonitor;
	int a, area = 0;

	for (m = monitors; m; m = m->next)
		if ((a = INTERSECT(x, y, w, h, m)) > area) {
			area = a;
			r = m;
		}
	return r;
}

void
resize(Client *c, int x, int y, int w, int h, int interact)
{
	if (applysizehints(c, &x, &y, &w, &h, interact))
		resizeclient(c, x, y, w, h);
}

void
resizeclient(Client *c, int x, int y, int w, int h)
{
	XWindowChanges wc;

	c->oldx = c->x; c->x = wc.x = x;
	c->oldy = c->y; c->y = wc.y = y;
	c->oldw = c->w; c->w = wc.width = w;
	c->oldh = c->h; c->h = wc.height = h;
	wc.border_width = c->borderWidth;
	if (((nexttiled(c->monitor->clients) == c && !nexttiled(c->next))
	    || &monocle == c->monitor->layouts[c->monitor->selectedLayout]->arrange)
        && !c->isFullscreen && !c->isFloating
        && NULL != c->monitor->layouts[c->monitor->selectedLayout]->arrange) {
		c->w = wc.width += c->borderWidth * 2;
		c->h = wc.height += c->borderWidth * 2;
		wc.border_width = 0;
	}
	XConfigureWindow(display, c->window, CWX | CWY | CWWidth | CWHeight | CWBorderWidth, &wc);
	configure(c);
	XSync(display, False);
}

void
resizemouse(const Argument *arg)
{
	int ocx, ocy, nw, nh;
	Client *c;
	Monitor *m;
	XEvent ev;
	Time lasttime = 0;

	if (!(c = selectedMonitor->selectedClient))
		return;
	if (c->isFullscreen) /* no support resizing fullscreen windows by mouse */
		return;
	restack(selectedMonitor);
	ocx = c->x;
	ocy = c->y;
	if (XGrabPointer(display, root, False, MOUSEMASK, GrabModeAsync, GrabModeAsync,
                     None, cursor[CurResize]->cursor, CurrentTime) != GrabSuccess)
		return;
	XWarpPointer(display, None, c->window, 0, 0, 0, 0, c->w + c->borderWidth - 1, c->h + c->borderWidth - 1);
	do {
		XMaskEvent(display, MOUSEMASK | ExposureMask | SubstructureRedirectMask, &ev);
		switch(ev.type) {
		case ConfigureRequest:
		case Expose:
		case MapRequest:
			handler[ev.type](&ev);
			break;
		case MotionNotify:
			if ((ev.xmotion.time - lasttime) <= (1000 / 60))
				continue;
			lasttime = ev.xmotion.time;

			nw = MAX(ev.xmotion.x - ocx - 2 * c->borderWidth + 1, 1);
			nh = MAX(ev.xmotion.y - ocy - 2 * c->borderWidth + 1, 1);
			if (c->monitor->windowX + nw >= selectedMonitor->windowX && c->monitor->windowX + nw <= selectedMonitor->windowX + selectedMonitor->windowWidth
                && c->monitor->windowY + nh >= selectedMonitor->windowY && c->monitor->windowY + nh <= selectedMonitor->windowY + selectedMonitor->windowHeight)
			{
				if (!c->isFloating && selectedMonitor->layouts[selectedMonitor->selectedLayout]->arrange
                    && (abs(nw - c->w) > snap || abs(nh - c->h) > snap))
					togglefloating(NULL);
			}
			if (!selectedMonitor->layouts[selectedMonitor->selectedLayout]->arrange || c->isFloating)
				resize(c, c->x, c->y, nw, nh, 1);
			break;
		}
	} while (ev.type != ButtonRelease);
	XWarpPointer(display, None, c->window, 0, 0, 0, 0, c->w + c->borderWidth - 1, c->h + c->borderWidth - 1);
	XUngrabPointer(display, CurrentTime);
	while (XCheckMaskEvent(display, EnterWindowMask, &ev));
	if ((m = rectangleToMonitor(c->x, c->y, c->w, c->h)) != selectedMonitor) {
		sendmon(c, m);
        selectedMonitor = m;
		focus(NULL);
	}
}

void
restack(Monitor *m)
{
	Client *c;
	XEvent ev;
	XWindowChanges wc;

    drawBar(m);
	if (!m->selectedClient)
		return;
	if (m->selectedClient->isFloating || !m->layouts[m->selectedLayout]->arrange)
		XRaiseWindow(display, m->selectedClient->window);
	if (m->layouts[m->selectedLayout]->arrange) {
		wc.stack_mode = Below;
		wc.sibling = m->barWindow;
		for (c = m->stack; c; c = c->selectionNext)
			if (!c->isFloating && ISVISIBLE(c)) {
				XConfigureWindow(display, c->window, CWSibling | CWStackMode, &wc);
				wc.sibling = c->window;
			}
	}
	XSync(display, False);
	while (XCheckMaskEvent(display, EnterWindowMask, &ev));
}

void run(void) {
	XEvent event;
	/* Main event loop */
	XSync(display, False);
	while (running && !XNextEvent(display, &event)) { // Loop through the X event queue
        if (handler[event.type]) { // If a handler exists for the event type
            handler[event.type](&event); // Call the event handler
        }
    }
}

void scan(void) {
    unsigned int i, num;
	Window d1, d2, *wins = NULL;
	XWindowAttributes wa;

	if (XQueryTree(display, root, &d1, &d2, &wins, &num)) {
		for (i = 0; i < num; i++) {
			if (!XGetWindowAttributes(display, wins[i], &wa)
			|| wa.override_redirect || XGetTransientForHint(display, wins[i], &d1))
				continue;
			if (wa.map_state == IsViewable || getstate(wins[i]) == IconicState)
				manage(wins[i], &wa);
		}
		for (i = 0; i < num; i++) { /* now the transients */
			if (!XGetWindowAttributes(display, wins[i], &wa))
				continue;
			if (XGetTransientForHint(display, wins[i], &d1)
			&& (wa.map_state == IsViewable || getstate(wins[i]) == IconicState))
				manage(wins[i], &wa);
		}
		if (wins)
			XFree(wins);
	}
}

void
sendmon(Client *c, Monitor *m)
{
	if (c->monitor == m)
		return;
	unfocus(c, 1);
	detach(c);
    detachStack(c);
	c->monitor = m;
	c->tags = m->tagSet[m->selectedTags]; /* assign tags of target monitor */
	attachBelow(c);
    attachStack(c);
	focus(NULL);
	arrange(NULL);
}

void
setclientstate(Client *c, long state)
{
	long data[] = { state, None };

	XChangeProperty(display, c->window, wmatom[WMState], wmatom[WMState], 32,
                    PropModeReplace, (unsigned char *)data, 2);
}

int
sendevent(Client *c, Atom proto)
{
	int n;
	Atom *protocols;
	int exists = 0;
	XEvent ev;

	if (XGetWMProtocols(display, c->window, &protocols, &n)) {
		while (!exists && n--)
			exists = protocols[n] == proto;
		XFree(protocols);
	}
	if (exists) {
		ev.type = ClientMessage;
		ev.xclient.window = c->window;
		ev.xclient.message_type = wmatom[WMProtocols];
		ev.xclient.format = 32;
		ev.xclient.data.l[0] = proto;
		ev.xclient.data.l[1] = CurrentTime;
		XSendEvent(display, c->window, False, NoEventMask, &ev);
	}
	return exists;
}

void setFocus(Client *c) {
	if (!c->neverFocus) {
		XSetInputFocus(display, c->window, RevertToPointerRoot, CurrentTime);
		XChangeProperty(display, root, netatom[NetActiveWindow],
                        XA_WINDOW, 32, PropModeReplace,
                        (unsigned char *) &(c->window), 1);
	}
	sendevent(c, wmatom[WMTakeFocus]);
}

void
setfullscreen(Client *c, int fullscreen)
{
	if (fullscreen && !c->isFullscreen) {
		XChangeProperty(display, c->window, netatom[NetWMState], XA_ATOM, 32,
                        PropModeReplace, (unsigned char*)&netatom[NetWMFullscreen], 1);
		c->isFullscreen = 1;
		c->oldState = c->isFloating;
		c->oldBorderWidth = c->borderWidth;
		c->borderWidth = 0;
		c->isFloating = 1;
		resizeclient(c, c->monitor->monitorX, c->monitor->monitorY, c->monitor->monitorWidth, c->monitor->monitorHeight);
		XRaiseWindow(display, c->window);
	} else if (!fullscreen && c->isFullscreen){
		XChangeProperty(display, c->window, netatom[NetWMState], XA_ATOM, 32,
                        PropModeReplace, (unsigned char*)0, 0);
		c->isFullscreen = 0;
		c->isFloating = c->oldState;
		c->borderWidth = c->oldBorderWidth;
		c->x = c->oldx;
		c->y = c->oldy;
		c->w = c->oldw;
		c->h = c->oldh;
		resizeclient(c, c->x, c->y, c->w, c->h);
		arrange(c->monitor);
	}
}

void
setlayout(const Argument *arg)
{
	if (!arg || !arg->v || arg->v != selectedMonitor->layouts[selectedMonitor->selectedLayout])
        selectedMonitor->selectedLayout ^= 1;
	if (arg && arg->v)
        selectedMonitor->layouts[selectedMonitor->selectedLayout] = (Layout *)arg->v;
	strncpy(selectedMonitor->layoutSymbol, selectedMonitor->layouts[selectedMonitor->selectedLayout]->symbol, sizeof selectedMonitor->layoutSymbol);
	if (selectedMonitor->selectedClient)
		arrange(selectedMonitor);
	else
        drawBar(selectedMonitor);
}

/* argument > 1.0 will set masterFactor absolutely */
void
setmfact(const Argument *arg)
{
	float f;

	if (!arg || !selectedMonitor->layouts[selectedMonitor->selectedLayout]->arrange)
		return;
	f = arg->f < 1.0 ? arg->f + selectedMonitor->masterFactor : arg->f - 1.0;
	if (f < 0.05 || f > 0.95)
		return;
    selectedMonitor->masterFactor = f;
	arrange(selectedMonitor);
}

void setup(void) {
	int i;
	XSetWindowAttributes windowAttributes;
	Atom utf8String;

	sigchld(0); // Clean up any zombies immediately

	/* Initialize screen */
	screen = DefaultScreen(display);
    screenWidth = DisplayWidth(display, screen);
    screenHeight = DisplayHeight(display, screen);
	root = RootWindow(display, screen);
    draw = drawCreate(display, screen, root, screenWidth, screenHeight);
	if (!drawFontsetCreate(draw, fonts, LENGTH(fonts)))
		die("No fonts could be loaded.");
    leftRightPad = draw->fonts->height;
    barHeight = draw->fonts->height + 2;
    updateGeometry();
	/* init atoms */
	utf8String = XInternAtom(display, "UTF8_STRING", False);
	wmatom[WMProtocols] = XInternAtom(display, "WM_PROTOCOLS", False);
	wmatom[WMDelete] = XInternAtom(display, "WM_DELETE_WINDOW", False);
	wmatom[WMState] = XInternAtom(display, "WM_STATE", False);
	wmatom[WMTakeFocus] = XInternAtom(display, "WM_TAKE_FOCUS", False);
	netatom[NetActiveWindow] = XInternAtom(display, "_NET_ACTIVE_WINDOW", False);
	netatom[NetSupported] = XInternAtom(display, "_NET_SUPPORTED", False);
	netatom[NetWMName] = XInternAtom(display, "_NET_WM_NAME", False);
	netatom[NetWMState] = XInternAtom(display, "_NET_WM_STATE", False);
	netatom[NetWMCheck] = XInternAtom(display, "_NET_SUPPORTING_WM_CHECK", False);
	netatom[NetWMFullscreen] = XInternAtom(display, "_NET_WM_STATE_FULLSCREEN", False);
	netatom[NetWMWindowType] = XInternAtom(display, "_NET_WM_WINDOW_TYPE", False);
	netatom[NetWMWindowTypeDialog] = XInternAtom(display, "_NET_WM_WINDOW_TYPE_DIALOG", False);
	netatom[NetClientList] = XInternAtom(display, "_NET_CLIENT_LIST", False);
	/* init cursors */
	cursor[CurNormal] = drw_cur_create(draw, XC_left_ptr);
	cursor[CurResize] = drw_cur_create(draw, XC_sizing);
	cursor[CurMove] = drw_cur_create(draw, XC_fleur);
	/* init appearance */
	scheme = ecalloc(LENGTH(colors), sizeof(Color *));
	for (i = 0; i < LENGTH(colors); i++)
		scheme[i] = drw_scm_create(draw, colors[i], 3);
	/* init bars */
	updatebars();
	updatestatus();
	/* supporting window for NetWMCheck */
	wmcheckwin = XCreateSimpleWindow(display, root, 0, 0, 1, 1, 0, 0, 0);
	XChangeProperty(display, wmcheckwin, netatom[NetWMCheck], XA_WINDOW, 32,
                    PropModeReplace, (unsigned char *) &wmcheckwin, 1);
	XChangeProperty(display, wmcheckwin, netatom[NetWMName], utf8String, 8,
                    PropModeReplace, (unsigned char *) "dwm", 3);
	XChangeProperty(display, root, netatom[NetWMCheck], XA_WINDOW, 32,
                    PropModeReplace, (unsigned char *) &wmcheckwin, 1);
	/* EWMH support per view */
	XChangeProperty(display, root, netatom[NetSupported], XA_ATOM, 32,
                    PropModeReplace, (unsigned char *) netatom, NetLast);
	XDeleteProperty(display, root, netatom[NetClientList]);
	/* select events */
	windowAttributes.cursor = cursor[CurNormal]->cursor;
    windowAttributes.event_mask = SubstructureRedirectMask | SubstructureNotifyMask
                                  | ButtonPressMask | PointerMotionMask | EnterWindowMask
                                  | LeaveWindowMask | StructureNotifyMask | PropertyChangeMask;
	XChangeWindowAttributes(display, root, CWEventMask | CWCursor, &windowAttributes);
	XSelectInput(display, root, windowAttributes.event_mask);
	grabkeys();
	focus(NULL);
}


void
seturgent(Client *c, int urg)
{
	XWMHints *wmh;

	c->isUrgent = urg;
	if (!(wmh = XGetWMHints(display, c->window)))
		return;
	wmh->flags = urg ? (wmh->flags | XUrgencyHint) : (wmh->flags & ~XUrgencyHint);
	XSetWMHints(display, c->window, wmh);
	XFree(wmh);
}

void
showhide(Client *c)
{
	if (!c)
		return;
	if (ISVISIBLE(c)) {
		/* show clients top down */
		XMoveWindow(display, c->window, c->x, c->y);
		if ((!c->monitor->layouts[c->monitor->selectedLayout]->arrange || c->isFloating) && !c->isFullscreen)
			resize(c, c->x, c->y, c->w, c->h, 0);
		showhide(c->selectionNext);
	} else {
		/* hide clients bottom up */
		showhide(c->selectionNext);
		XMoveWindow(display, c->window, WIDTH(c) * -2, c->y);
	}
}

void sigchld(int unused) {
	if (signal(SIGCHLD, sigchld) == SIG_ERR) // Set the handler for the signal SIGCHLD to sigchld, returning the old handler, or SIG_ERR on error, whatever this means...
		die("Can't install SIGCHLD handler:"); // Print error and exit
	while (0 < waitpid(-1, NULL, WNOHANG)); // Wait for any process to die (?)
}

void spawn(const Argument *argument) {
    /* If the command is the dmenu command, set the dmenu monitor to pass it as an argument */
    if (argument->v == dmenuCommand) {
        dmenuMonitor[0] = '0' + selectedMonitor->num;
    }
    /* Create a new process by duplicating the calling process (dwm)
     * The new process is referred to as the child process
     * The calling process is referred to as the parent process
     * The child process will have the PID of the old calling process
     * The PID of the parent process changes
     * It only returns 0 for the child process, the parent process returns its new PID,
     * so only the child process enters the conditional */
	if (fork() == 0) {
		if (display)
			close(ConnectionNumber(display));
        /* Create a new session with the calling process as its leader.
         * The process group IDs of the session and the calling process
         * are set to the process ID of the calling process, which is returned.
         * This detaches the child from the parent, as I understand. */
		setsid();
        /* Replace the calling process with a new one, called like (command, arguments)
         * The first element of the arguments is the command itself
         * The arguments must be NULL terminated
         * argument is converted to a pointer to a pointer because it is an array of strings (array of arrays)
         * The "v" in execvp stands for "vector", because we pass the arguments as a vector
         * The "p" in execvp stands for "path", because we use the system path in the call */
		execvp(((char **)argument->v)[0], (char **)argument->v);
        /* These lines will only be reached if the exec call fails, since the process won't be replaced */
		fprintf(stderr, "dwm: execvp %s", ((char **)argument->v)[0]);
		perror(" failed");
		exit(EXIT_SUCCESS);
	}
}

void
tag(const Argument *arg)
{
	if (selectedMonitor->selectedClient && arg->ui & TAGMASK) {
        selectedMonitor->selectedClient->tags = arg->ui & TAGMASK;
		focus(NULL);
		arrange(selectedMonitor);
	}
}

void
tagmon(const Argument *arg)
{
	if (!selectedMonitor->selectedClient || !monitors->next)
		return;
	sendmon(selectedMonitor->selectedClient, dirtomon(arg->i));
}

void
tile(Monitor *m)
{
	unsigned int i, n, h, mw, my, ty;
	Client *c;

	for (n = 0, c = nexttiled(m->clients); c; c = nexttiled(c->next), n++);
	if (n == 0)
		return;

	if (n > m->nMaster)
		mw = m->nMaster ? m->windowWidth * m->masterFactor : 0;
	else
		mw = m->windowWidth;
	for (i = my = ty = 0, c = nexttiled(m->clients); c; c = nexttiled(c->next), i++)
		if (i < m->nMaster) {
			h = (m->windowHeight - my) / (MIN(n, m->nMaster) - i);
			resize(c, m->windowX, m->windowY + my, mw - (2 * c->borderWidth), h - (2 * c->borderWidth), 0);
			if (my + HEIGHT(c) < m->windowHeight)
				my += HEIGHT(c);
		} else {
			h = (m->windowHeight - ty) / (n - i);
			resize(c, m->windowX + mw, m->windowY + ty, m->windowWidth - mw - (2 * c->borderWidth), h - (2 * c->borderWidth), 0);
			if (ty + HEIGHT(c) < m->windowHeight)
				ty += HEIGHT(c);
		}
}

void
dwindle(Monitor *mon) {
	unsigned int i, n, nx, ny, nw, nh;
	Client *c;

	for(n = 0, c = nexttiled(mon->clients); c; c = nexttiled(c->next), n++);
	if(n == 0)
		return;

	nx = mon->windowX;
	ny = 0;
	nw = mon->windowWidth;
	nh = mon->windowHeight;

	for(i = 0, c = nexttiled(mon->clients); c; c = nexttiled(c->next)) {
		if((i % 2 && nh / 2 > 2 * c->borderWidth)
		   || (!(i % 2) && nw / 2 > 2 * c->borderWidth)) {
			if(i < n - 1) {
				if(i % 2)
					nh /= 2;
				else
					nw /= 2;
			}
			if((i % 4) == 0) {
				ny += nh;
			}
			else if((i % 4) == 1)
				nx += nw;
			else if((i % 4) == 2)
				ny += nh;
			else if((i % 4) == 3) {
				nx += nw;
			}
			if(i == 0)
			{
				if(n != 1)
					nw = mon->windowWidth * mon->masterFactor;
				ny = mon->windowY;
			}
			else if(i == 1)
				nw = mon->windowWidth - nw;
			i++;
		}
		resize(c, nx, ny, nw - 2 * c->borderWidth, nh - 2 * c->borderWidth, False);
	}
}

void toggleBar(const Argument *argument) {
    selectedMonitor->showBar = !selectedMonitor->showBar;
	updatebarpos(selectedMonitor);
	XMoveResizeWindow(display, selectedMonitor->barWindow, selectedMonitor->windowX, selectedMonitor->by, selectedMonitor->windowWidth, barHeight);
	arrange(selectedMonitor);
}

void
togglefloating(const Argument *arg)
{
	if (!selectedMonitor->selectedClient)
		return;
	if (selectedMonitor->selectedClient->isFullscreen) /* no support for fullscreen windows */
		return;
    selectedMonitor->selectedClient->isFloating = !selectedMonitor->selectedClient->isFloating || selectedMonitor->selectedClient->isFixed;
	if (selectedMonitor->selectedClient->isFloating)
		resize(selectedMonitor->selectedClient, selectedMonitor->selectedClient->x, selectedMonitor->selectedClient->y,
               selectedMonitor->selectedClient->w, selectedMonitor->selectedClient->h, 0);
	arrange(selectedMonitor);
}

void
toggletag(const Argument *arg)
{
	unsigned int newtags;

	if (!selectedMonitor->selectedClient)
		return;
	newtags = selectedMonitor->selectedClient->tags ^ (arg->ui & TAGMASK);
	if (newtags) {
        selectedMonitor->selectedClient->tags = newtags;
		focus(NULL);
		arrange(selectedMonitor);
	}
}

void
toggleview(const Argument *arg)
{
	unsigned int newtagset = selectedMonitor->tagSet[selectedMonitor->selectedTags] ^ (arg->ui & TAGMASK);

	if (newtagset) {
        selectedMonitor->tagSet[selectedMonitor->selectedTags] = newtagset;
		focus(NULL);
		arrange(selectedMonitor);
	}
}

void
unfocus(Client *c, int setfocus)
{
	if (!c)
		return;
    grabButtons(c, 0);
	XSetWindowBorder(display, c->window, scheme[SchemeNorm][ColBorder].pixel);
	if (setfocus) {
		XSetInputFocus(display, root, RevertToPointerRoot, CurrentTime);
		XDeleteProperty(display, root, netatom[NetActiveWindow]);
	}
}

void
unmanage(Client *c, int destroyed)
{
	Monitor *m = c->monitor;
	XWindowChanges wc;

	detach(c);
    detachStack(c);
	if (!destroyed) {
		wc.border_width = c->oldBorderWidth;
		XGrabServer(display); /* avoid race conditions */
		XSetErrorHandler(xerrordummy);
		XConfigureWindow(display, c->window, CWBorderWidth, &wc); /* restore border */
		XUngrabButton(display, AnyButton, AnyModifier, c->window);
		setclientstate(c, WithdrawnState);
		XSync(display, False);
		XSetErrorHandler(xerror);
		XUngrabServer(display);
	}
	free(c);
	focus(NULL);
	updateclientlist();
	arrange(m);
}

void
unmapnotify(XEvent *e)
{
	Client *c;
	XUnmapEvent *ev = &e->xunmap;

	if ((c = windowToClient(ev->window))) {
		if (ev->send_event)
			setclientstate(c, WithdrawnState);
		else
			unmanage(c, 0);
	}
}

void
updatebars(void)
{
	Monitor *m;
	XSetWindowAttributes wa = {
		.override_redirect = True,
		.background_pixmap = ParentRelative,
		.event_mask = ButtonPressMask|ExposureMask
	};
	XClassHint ch = {"dwm", "dwm"};
	for (m = monitors; m; m = m->next) {
		if (m->barWindow)
			continue;
		m->barWindow = XCreateWindow(display, root, m->windowX, m->by, m->windowWidth, barHeight, 0, DefaultDepth(display, screen),
                                     CopyFromParent, DefaultVisual(display, screen),
				CWOverrideRedirect|CWBackPixmap|CWEventMask, &wa);
		XDefineCursor(display, m->barWindow, cursor[CurNormal]->cursor);
		XMapRaised(display, m->barWindow);
		XSetClassHint(display, m->barWindow, &ch);
	}
}

void
updatebarpos(Monitor *m)
{
	m->windowY = m->monitorY;
	m->windowHeight = m->monitorHeight;
	if (m->showBar) {
		m->windowHeight -= barHeight;
		m->by = m->topBar ? m->windowY : m->windowY + m->windowHeight;
		m->windowY = m->topBar ? m->windowY + barHeight : m->windowY;
	} else
		m->by = -barHeight;
}

void
updateclientlist()
{
	Client *c;
	Monitor *m;

	XDeleteProperty(display, root, netatom[NetClientList]);
	for (m = monitors; m; m = m->next)
		for (c = m->clients; c; c = c->next)
			XChangeProperty(display, root, netatom[NetClientList],
                            XA_WINDOW, 32, PropModeAppend,
                            (unsigned char *) &(c->window), 1);
}

int updateGeometry(void) {
	int dirty = 0;

#ifdef XINERAMA
	if (XineramaIsActive(display)) {
		int i, j, n, nn;
		Client *c;
		Monitor *m;
		XineramaScreenInfo *info = XineramaQueryScreens(display, &nn);
		XineramaScreenInfo *unique = NULL;

		for (n = 0, m = monitors; m; m = m->next, n++);
		/* only consider unique geometries as separate screens */
		unique = ecalloc(nn, sizeof(XineramaScreenInfo));
		for (i = 0, j = 0; i < nn; i++)
			if (isuniquegeom(unique, j, &info[i]))
				memcpy(&unique[j++], &info[i], sizeof(XineramaScreenInfo));
		XFree(info);
		nn = j;
		if (n <= nn) { /* new monitors available */
			for (i = 0; i < (nn - n); i++) {
				for (m = monitors; m && m->next; m = m->next);
				if (m)
					m->next = createMonitor();
				else
                    monitors = createMonitor();
			}
			for (i = 0, m = monitors; i < nn && m; m = m->next, i++)
				if (i >= n
                    || unique[i].x_org != m->monitorX || unique[i].y_org != m->monitorY
                    || unique[i].width != m->monitorWidth || unique[i].height != m->monitorHeight)
				{
					dirty = 1;
					m->num = i;
                    m->monitorX = m->windowX = unique[i].x_org;
                    m->monitorY = m->windowY = unique[i].y_org;
                    m->monitorWidth = m->windowWidth = unique[i].width;
                    m->monitorHeight = m->windowHeight = unique[i].height;
					updatebarpos(m);
				}
		} else { /* less monitors available nn < n */
			for (i = nn; i < n; i++) {
				for (m = monitors; m && m->next; m = m->next);
				while ((c = m->clients)) {
					dirty = 1;
					m->clients = c->next;
                    detachStack(c);
					c->monitor = monitors;
					attach(c);
					attachBelow(c);
                    attachStack(c);
				}
				if (m == selectedMonitor)
                    selectedMonitor = monitors;
				cleanupmon(m);
			}
		}
		free(unique);
	} else
#endif /* XINERAMA */
	{ /* default monitor setup */
		if (!monitors)
            monitors = createMonitor();
		if (monitors->monitorWidth != screenWidth || monitors->monitorHeight != screenHeight) {
			dirty = 1;
            monitors->monitorWidth = monitors->windowWidth = screenWidth;
            monitors->monitorHeight = monitors->windowHeight = screenHeight;
			updatebarpos(monitors);
		}
	}
	if (dirty) {
        selectedMonitor = monitors;
        selectedMonitor = windowToMonitor(root);
	}
	return dirty;
}

void
updatenumlockmask(void)
{
	unsigned int i, j;
	XModifierKeymap *modmap;

	numlockmask = 0;
	modmap = XGetModifierMapping(display);
	for (i = 0; i < 8; i++)
		for (j = 0; j < modmap->max_keypermod; j++)
			if (modmap->modifiermap[i * modmap->max_keypermod + j]
				== XKeysymToKeycode(display, XK_Num_Lock))
				numlockmask = (1 << i);
	XFreeModifiermap(modmap);
}

void
updatesizehints(Client *c)
{
	long msize;
	XSizeHints size;

	if (!XGetWMNormalHints(display, c->window, &size, &msize))
		/* size is uninitialized, ensure that size.flags aren't used */
		size.flags = PSize;
	if (size.flags & PBaseSize) {
		c->basew = size.base_width;
		c->baseh = size.base_height;
	} else if (size.flags & PMinSize) {
		c->basew = size.min_width;
		c->baseh = size.min_height;
	} else
		c->basew = c->baseh = 0;
	if (size.flags & PResizeInc) {
		c->incw = size.width_inc;
		c->inch = size.height_inc;
	} else
		c->incw = c->inch = 0;
	if (size.flags & PMaxSize) {
		c->maxw = size.max_width;
		c->maxh = size.max_height;
	} else
		c->maxw = c->maxh = 0;
	if (size.flags & PMinSize) {
		c->minw = size.min_width;
		c->minh = size.min_height;
	} else if (size.flags & PBaseSize) {
		c->minw = size.base_width;
		c->minh = size.base_height;
	} else
		c->minw = c->minh = 0;
	if (size.flags & PAspect) {
		c->mina = (float)size.min_aspect.y / size.min_aspect.x;
		c->maxa = (float)size.max_aspect.x / size.max_aspect.y;
	} else
		c->maxa = c->mina = 0.0;
	c->isFixed = (c->maxw && c->maxh && c->maxw == c->minw && c->maxh == c->minh);
}

void
updatestatus(void)
{
	if (!gettextprop(root, XA_WM_NAME, statusText, sizeof(statusText)))
		strcpy(statusText, "dwm-"VERSION);
    drawBar(selectedMonitor);
}

void
updatetitle(Client *c)
{
	if (!gettextprop(c->window, netatom[NetWMName], c->name, sizeof c->name))
		gettextprop(c->window, XA_WM_NAME, c->name, sizeof c->name);
	if (c->name[0] == '\0') /* hack to mark broken clients */
		strcpy(c->name, broken);
}

void
updatewindowtype(Client *c)
{
	Atom state = getatomprop(c, netatom[NetWMState]);
	Atom wtype = getatomprop(c, netatom[NetWMWindowType]);

	if (state == netatom[NetWMFullscreen])
		setfullscreen(c, 1);
	if (wtype == netatom[NetWMWindowTypeDialog])
		c->isFloating = 1;
}

void
updatewmhints(Client *c)
{
	XWMHints *wmh;

	if ((wmh = XGetWMHints(display, c->window))) {
		if (c == selectedMonitor->selectedClient && wmh->flags & XUrgencyHint) {
			wmh->flags &= ~XUrgencyHint;
			XSetWMHints(display, c->window, wmh);
		} else
			c->isUrgent = (wmh->flags & XUrgencyHint) ? 1 : 0;
		if (wmh->flags & InputHint)
			c->neverFocus = !wmh->input;
		else
			c->neverFocus = 0;
		XFree(wmh);
	}
}

void
view(const Argument *arg)
{
	if ((arg->ui & TAGMASK) == selectedMonitor->tagSet[selectedMonitor->selectedTags])
		return;
    selectedMonitor->selectedTags ^= 1; /* toggle selectedClient tagSet */
	if (arg->ui & TAGMASK)
        selectedMonitor->tagSet[selectedMonitor->selectedTags] = arg->ui & TAGMASK;
	focus(NULL);
	arrange(selectedMonitor);
}

Client *windowToClient(Window window) {
	Client *c;
	Monitor *m;

	for (m = monitors; m; m = m->next)
		for (c = m->clients; c; c = c->next)
			if (c->window == window)
				return c;
	return NULL;
}

Monitor *windowToMonitor(Window window) {
	int x, y;
	Client *client;
	Monitor *monitor;

    if (window == root && getRootPointer(&x, &y)) {
        return rectangleToMonitor(x, y, 1, 1);
    }
    for (monitor = monitors; monitor; monitor = monitor->next) {
        if (window == monitor->barWindow) {
            return monitor;
        }
    }
    if ((client = windowToClient(window))) {
        return client->monitor;
    }
    return selectedMonitor;
}

/* There's no way to check accesses to destroyed windows, thus those cases are
 * ignored (especially on UnmapNotify's). Other types of errors call Xlibs
 * default error handler, which may call exit. */
int
xerror(Display *dpy, XErrorEvent *ee)
{
	if (ee->error_code == BadWindow
	|| (ee->request_code == X_SetInputFocus && ee->error_code == BadMatch)
	|| (ee->request_code == X_PolyText8 && ee->error_code == BadDrawable)
	|| (ee->request_code == X_PolyFillRectangle && ee->error_code == BadDrawable)
	|| (ee->request_code == X_PolySegment && ee->error_code == BadDrawable)
	|| (ee->request_code == X_ConfigureWindow && ee->error_code == BadMatch)
	|| (ee->request_code == X_GrabButton && ee->error_code == BadAccess)
	|| (ee->request_code == X_GrabKey && ee->error_code == BadAccess)
	|| (ee->request_code == X_CopyArea && ee->error_code == BadDrawable))
		return 0;
	fprintf(stderr, "dwm: fatal error: request code=%d, error code=%d\n",
		ee->request_code, ee->error_code);
	return xerrorxlib(dpy, ee); /* may call exit */
}

int
xerrordummy(Display *dpy, XErrorEvent *ee)
{
	return 0;
}

/* Startup Error handler to check if another window manager
 * is already running. */
int
xerrorstart(Display *dpy, XErrorEvent *ee)
{
	die("dwm: another window manager is already running");
	return -1;
}

void
zoom(const Argument *arg)
{
	Client *c = selectedMonitor->selectedClient;

	if (!selectedMonitor->layouts[selectedMonitor->selectedLayout]->arrange
	|| (selectedMonitor->selectedClient && selectedMonitor->selectedClient->isFloating))
		return;
	if (c == nexttiled(selectedMonitor->clients))
		if (!c || !(c = nexttiled(c->next)))
			return;
	pop(c);
}

int main(int argc, char *argv[]) { // Entrypoint
    if (argc == 2 && !strcmp("-v", argv[1])) {// If there are exactly two arguments and the second one is "-v"
        die("dwm-"VERSION); // Print version and exit
    } else if (argc != 1) { // Else if the argument count is not 1 (i.e. is greater than 2)
        die("usage: dwm [-v]"); // Print usage and exit
    }
    if (!setlocale(LC_CTYPE, "") || !XSupportsLocale()) { // Set locale from $LAND environment variable and checks if X can operate using the current locale
        fputs("warning: no locale support\n", stderr); // Print error and exit
    }
    if (!(display = XOpenDisplay(NULL))) { // Connect to the X display server
        die("dwm: cannot open display"); // Print error and exit
    }
    checkOtherWindowManager(); // This will throw an error if another window manager is running
	setup();
#ifdef __OpenBSD__
	if (pledge("stdio rpath proc exec", NULL) == -1)
		die("pledge");
#endif /* __OpenBSD__ */
	scan(); // Check if other programs are running, so that they can be added to DWM when launched
	run(); // Main program
	cleanup();
	XCloseDisplay(display);
	return EXIT_SUCCESS;
}
