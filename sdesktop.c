/*
 *  sdesktop.c
 *
 *  Copyright (c) 2009-2011 Tuxce <tuxce.net@gmail.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>

extern char *optarg;
extern int optind;
extern int errno;

#define _NAME "sdesktop"
#define _VERSION "1.0"


#define NB_DESKTOP     "_NET_NUMBER_OF_DESKTOPS"
#define CUR_DESKTOP    "_NET_CURRENT_DESKTOP"
#define ACTIVE_WINDOW  "_NET_ACTIVE_WINDOW"
#define NET_STACK      "_NET_CLIENT_LIST_STACKING"
#define NET_WM_DESKTOP "_NET_WM_DESKTOP"

#define BTN_UP   Button4  /* mouse wheel up */
#define BTN_DOWN Button5  /* mouse wheel down */
#define BTN_NEXT 6        /* mouse wheel right */
#define BTN_PREV 7        /* mouse wheel left */

/* default window for handling mouse wheel */
#define WM_DESKTOP "root"

/* sleep time before next event handling in 1E-6 secs */
#define WAIT_TIME 100000

/* how many times should we try to find window */
#define TRIES 3
#define TRY_SLEEP 2

/* Errors */
#define ERR_OPT   1
#define ERR_FORK  2
#define ERR_ATOMS 3
#define ERR_WINS  4

/* exit program on terminate=1 */
int terminate=0;

Atom a_net_stack;
Atom a_net_wm_desktop;


long get_win_prop (Display *d, Window w, Atom a, Atom req_type)
{
	long ret=-1;
	Atom a_type;
	int a_format;
	unsigned long nitems, bytes;
	unsigned char *data;

	if (XGetWindowProperty(d, w, a, 0, 1, False, req_type,
		&a_type, &a_format, &nitems, &bytes, &data) == Success)
	{
		if (nitems != 0 && a_format == 32)
		{
			ret = *((long *) data);
		}
		XFree (data);
	}
	return ret;
}

Window get_below_win (Display *d, Window r, Window w, int first)
{
	Window ret=None;
	Atom a_type;
	int a_format;
	int i, w_found = 0;
	unsigned long nitems, bytes;
	unsigned char *data;
	long w_desktop = get_win_prop (d, w, a_net_wm_desktop, XA_CARDINAL);

	if (XGetWindowProperty(d, r, a_net_stack, 0, 8192L, False, XA_WINDOW,
		&a_type, &a_format, &nitems, &bytes, &data) == Success)
	{
		if (nitems != 0 && a_format == 32)
		{
			for (i=nitems-1; i>=0; i--)
			{
				Window w_prov = ((long *) data)[i];
				if (w_desktop != get_win_prop (d, w_prov,
				    a_net_wm_desktop, XA_CARDINAL))
					continue;
				if (w_found)
				{
					ret = w_prov;
					if (first) break;
				}
				if (w_prov == w) w_found = 1;
			}
		}
		XFree (data);
	}
	return ret;
}

void grab_btn (Display *d, Window w, unsigned int btn, unsigned int mod)
{
	if (btn==0) return;
	/* Grab action whatever the state of caps lock/num lock. */
	XGrabButton(d, btn, mod | Mod2Mask, w, False, ButtonPressMask,
                GrabModeAsync, GrabModeAsync,None, None);
	XGrabButton(d, btn, mod | LockMask, w, False, ButtonPressMask,
                GrabModeAsync, GrabModeAsync,None, None);
	XGrabButton(d, btn, mod | Mod2Mask | LockMask, w, False, ButtonPressMask,
                GrabModeAsync, GrabModeAsync,None, None);
	XGrabButton(d, btn, mod | 0, w, False, ButtonPressMask,
                GrabModeAsync, GrabModeAsync,None, None);
}

void ungrab_btn (Display *d, Window w, unsigned int btn)
{
	if (btn==0) return;
	/* Ungrab action */
	XUngrabButton (d, btn, AnyModifier, w);
}


/* fonction modifiée provenant de (XOrg) xprop/dsimple.c */
Window window_by (Display *d, Window r, const char *name, int by_name)
{
	if (strcmp (name, "root")==0) return r;
	Window *children, dummy;
	unsigned int nchildren;
	int i;
	Window w=0;
	XClassHint wclass;
	char *window_name;
	int found=0;

	if (by_name)
	{
		if (XFetchName(d, r, &window_name))
		{
			if (!strcmp(window_name, name)) found=1;
			XFree (window_name);
		}
	}
	else
	{
		if (XGetClassHint (d, r, &wclass))
		{
			if (!strcmp(wclass.res_name, name)) found=1;
			XFree (wclass.res_name);
			XFree (wclass.res_class);
		}
	}
	if (found)
	{
		return r;
	}

	if (!XQueryTree(d, r, &dummy, &dummy, &children, &nchildren))
	  return 0;

	for (i=0; i<nchildren; i++) {
		w = window_by (d, children[i], name, by_name);
		if (w)
		  break;
	}
	if (children) XFree ((char *)children);
	return w;
}

void end_program (int sig)
{
	terminate = 1;
}

void usage ()
{
	fprintf(stderr, "%s %s\n", _NAME, _VERSION);
	fprintf(stderr, "Switch desktop/window with mouse\n");
	fprintf(stderr, "Usage: %s [-options] [windows ...]\n", _NAME);
	fprintf(stderr, "\nwhere options include:");
	fprintf(stderr, "\n\t-a button to switch to next window (default: %d)", BTN_NEXT);
	fprintf(stderr, "\n\t-b button to switch to previous window (default: %d)", BTN_PREV);
	fprintf(stderr, "\n\t-c search by class (default)");
	fprintf(stderr, "\n\t-d set down button (default: %d)", BTN_DOWN);
	fprintf(stderr, "\n\t-f foreground");
	fprintf(stderr, "\n\t-g grab button if root window is selected");
	fprintf(stderr, "\n\t-n search by name");
	fprintf(stderr, "\n\t-u set up button (default: %d)", BTN_UP);
	fprintf(stderr, "\n\t-w scroll windows only");
	fprintf(stderr, "\n\ndefault window: %s", WM_DESKTOP);
	fprintf(stderr, "\n\n");
}

int main (int argc, char **argv)
{
	Display *display;
	Window root;
	Window active_win, bottom_win;
	Window *wins;
	Window *win;
	Atom a_nb_desktop, a_cur_desktop, a_active_window;
	XEvent xes, xeg;
	long nb_desktop, cur_desktop;
	unsigned int i,j;
	int by_name=0;
	pid_t pid;
	int foreground=0, grab=0, just_win=0;
	int opt;
	unsigned int btn_up=BTN_UP, btn_down=BTN_DOWN, btn_prev=BTN_PREV,
	             btn_next=BTN_NEXT;

	while ((opt = getopt (argc, argv, "a:b:cfghnu:d:w")) != -1)
	{
		switch (opt) 
		{
			case 'a':
				btn_next = atoi (optarg);
				break;
			case 'b':
				btn_prev = atoi (optarg);
				break;
			case 'c':
				by_name = 0;
				break;
			case 'd':
				btn_down = atoi (optarg);
				break;
			case 'f':
				foreground=1;
				break;
			case 'g':
				grab=1;
				break;
			case 'n':
				by_name=1;
				break;
			case 'u':
				btn_up = atoi (optarg);
				break;
			case 'w':
				just_win = 1;
				break;
			case 'h':
				usage ();
				return 0;
			default: /* '?' */
				usage ();
				return ERR_OPT;
		}
	}

	if (!foreground)
	{
		/* daemonize */
		if ((pid = fork ()) == -1)
			return ERR_FORK;
		if (pid)
		{
			return 0;
		}
	}

	/* handle signals */
	signal(SIGINT,&end_program);
	signal(SIGTERM,&end_program);

	display = XOpenDisplay(NULL);
	root = DefaultRootWindow(display);

	if ((a_nb_desktop = XInternAtom(display, NB_DESKTOP, True)) == None ||
		(a_cur_desktop = XInternAtom(display, CUR_DESKTOP, True)) == None ||
		(a_active_window = XInternAtom(display, ACTIVE_WINDOW, True)) == None  ||
		(a_net_wm_desktop = XInternAtom(display, NET_WM_DESKTOP, True)) == None  ||
		(a_net_stack = XInternAtom(display, NET_STACK, True)) == None )
	{
		return ERR_ATOMS;
	}

	/* search window(s) */
	if (argc > optind)
	{
		wins = (Window *) calloc (argc - optind, sizeof (Window));
		win = wins;
		for (i=optind; i<argc; i++)
		{
			j=0;
			while (!(*win = window_by (display, root, argv[i], by_name)) &&
				j++<TRIES)
				sleep (TRY_SLEEP);
			if (*win) win++;
		}
		*win = 0;
	}
	else
	{
		wins = (Window *) calloc (2, sizeof (Window));
		j=0;
		while (!(wins[0] = window_by (display, root, WM_DESKTOP, 0)) &&
			j++<TRIES)
			sleep (TRY_SLEEP);
		wins[1] = 0;
	}
	if (!wins[0])
	{
		free (wins);
		XCloseDisplay (display);
		return ERR_WINS;
	}

	/* grab actions */
	for (win=wins; *win!=0; win++)
	{
		if (*win==root && !grab)
			XSelectInput(display, *win, StructureNotifyMask | ButtonPressMask);
		else if (!just_win)
		{
			XSelectInput(display, *win, StructureNotifyMask);
			grab_btn (display, *win, btn_up, 0);
			grab_btn (display, *win, btn_down, 0);
		}
		grab_btn (display, *win, btn_prev, 0);
		grab_btn (display, *win, btn_next, 0);
		if (!just_win)
		{
			grab_btn (display, *win, btn_up, Mod1Mask);
			grab_btn (display, *win, btn_down, Mod1Mask);
		}
	}

	xes.type = ClientMessage;
	xes.xclient.type = ClientMessage; 
	xes.xclient.display = display;
	xes.xclient.format = 32;
	while (1)
	{
		/* wait for an event */
		if (XPending (display))
		{
			XNextEvent (display,&xeg);
			if (xeg.type == DestroyNotify) 
			{
				/* window destroyed */
				for (i=0; wins[i]!=0; i++)
				{
					if (wins[i]==xeg.xdestroywindow.window)
					{
						wins[i] = wins[i+1];
						if (wins[i+1]) 
							wins[i+1] = xeg.xdestroywindow.window;
					}
				}
				/* no more windows */
				if (!wins[0])
					break;
			}
			if (xeg.type != ButtonPress)
				continue;
		}
		else if (terminate)
			break;
		else 
		{
			usleep (WAIT_TIME);
			continue;
		}
		if (!just_win && (xeg.xbutton.button==btn_up || xeg.xbutton.button==btn_down))
		{
			nb_desktop = get_win_prop (display, root, a_nb_desktop, XA_CARDINAL);
			cur_desktop = get_win_prop (display, root, a_cur_desktop, XA_CARDINAL);
			if (xeg.xbutton.button==btn_up && --cur_desktop<0)
				cur_desktop = nb_desktop - 1;
			if (xeg.xbutton.button==btn_down && ++cur_desktop==nb_desktop)
				cur_desktop = 0;
			/* prepare the switch desktop event */
			xes.xclient.message_type = a_cur_desktop;
			xes.xclient.window = root;
			xes.xclient.data.l[0] = cur_desktop;
			xes.xclient.data.l[1] = xeg.xbutton.time;
			xes.xclient.data.l[2] = 0L;
		}
		else if (xeg.xbutton.button==btn_prev || xeg.xbutton.button==btn_next)
		{
			active_win = (Window) get_win_prop (display, root, a_active_window, XA_WINDOW);
			if (active_win == None) continue;
			bottom_win = get_below_win (display, root, active_win, (xeg.xbutton.button==btn_next));
			if (bottom_win == None) continue;
			if (xeg.xbutton.button==btn_next)
				XLowerWindow (display, active_win);
			xes.xclient.message_type = a_active_window;
			xes.xclient.window = bottom_win;
			xes.xclient.data.l[0] = 2L; /* simulate a pager request */
			xes.xclient.data.l[1] = xeg.xbutton.time;
			xes.xclient.data.l[2] = active_win;
		}
		else
			continue;
		XSendEvent(display, root, False, 
				 SubstructureNotifyMask | SubstructureRedirectMask,
				 &xes);
		//XSync (display, False);
	}

	/* ungrab actions */
	for (win=wins; *win!=0; win++)
	{
		if (!just_win && (*win!=root || grab))
		{
			ungrab_btn (display, *win, btn_up);
			ungrab_btn (display, *win, btn_down);
		}
		ungrab_btn (display, *win, btn_prev);
		ungrab_btn (display, *win, btn_next);
	}

	free (wins);
	XCloseDisplay (display);

	return 0;
}
