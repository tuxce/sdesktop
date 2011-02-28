/*
 *  sdesktop.c
 *
 *  Copyright (c) 2009-2010 Tuxce <tuxce.net@gmail.com>
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
#define _VERSION "0.4"


#define NB_DESKTOP 	"_NET_NUMBER_OF_DESKTOPS"
#define CUR_DESKTOP	"_NET_CURRENT_DESKTOP"

#define BTN_UP		Button4	/* mouse wheel up */
#define BTN_DOWN	Button5	/* mouse wheel down */

/* default window for handling mouse wheel */
#define WM_DESKTOP "desktop_window"	

/* sleep time before next event handling in 1E-6 secs */
#define WAIT_TIME 100000

/* how many times should we try to find window */
#define TRIES 3
#define TRY_SLEEP 2

/* exit program on terminate=1 */
int terminate=0;

long get_win_prop (Display *d, Window w, Atom a)
{
	long ret=-1;
	Atom a_type;
	int a_format;
	unsigned long nitems, bytes;
	unsigned char *data;

	if (XGetWindowProperty(d, w, a, 0, 1, False, XA_CARDINAL,
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

void grab_btn (Display *d, Window w, unsigned int btn)
{
	/* Grab action whatever the state of caps lock/num lock. */
	XGrabButton(d, btn, Mod2Mask, w, False, ButtonPressMask,
                GrabModeAsync, GrabModeAsync,None, None);
	XGrabButton(d, btn, LockMask, w, False, ButtonPressMask,
                GrabModeAsync, GrabModeAsync,None, None);
	XGrabButton(d, btn, Mod2Mask | LockMask, w, False, ButtonPressMask,
                GrabModeAsync, GrabModeAsync,None, None);
	XGrabButton(d, btn, 0, w, False, ButtonPressMask,
                GrabModeAsync, GrabModeAsync,None, None);
}

void ungrab_btn (Display *d, Window w, unsigned int btn)
{
	/* Ungrab action */
	XUngrabButton (d, btn, AnyModifier, w);
}


/* fonction modifiée provenant de (XOrg) xprop/dsimple.c */
Window window_by (Display *d, Window r, const char *name, int by_name)
{
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
		return (r);
	}

	if (!XQueryTree(d, r, &dummy, &dummy, &children, &nchildren))
	  return(0);

	for (i=0; i<nchildren; i++) {
		w = window_by (d, children[i], name, by_name);
		if (w)
		  break;
	}
	if (children) XFree ((char *)children);
	return(w);
}

void end_program (int sig)
{
	terminate = 1;
}
	
int main (int argc, char **argv)
{
	Display *display;
	Window root;
	Window *wins;
	Window *win;
	Atom a_nb_desktop;
	Atom a_cur_desktop;
	XEvent xes, xeg;
	long nb_desktop, cur_desktop;
	unsigned int i,j;
	int by_name=0;
	pid_t pid;
	int verbose=0, foreground=0, grab=0;
	int opt;
	unsigned int btn_up=BTN_UP, btn_down=BTN_DOWN;

	while ((opt = getopt (argc, argv, "cfghnvu:d:")) != -1)
	{
		switch (opt) 
		{
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
			case 'v':
				verbose=1;
				break;
			case 'h':
			default: /* '?' */
				fprintf(stderr, "%s %s\n", _NAME, _VERSION);
				fprintf(stderr, "Switch desktop with mouse wheel, grab action on nautilus desktop by default\n", argv[0]);
				fprintf(stderr, "Usage: %s [-options] [windows ...]\n", _NAME);
				fprintf(stderr, "\nwhere options include:");
				fprintf(stderr, "\n\t-d set down button (default: %d)", BTN_DOWN);
				fprintf(stderr, "\n\t-c search by class (default)");
				fprintf(stderr, "\n\t-n search by name");
				fprintf(stderr, "\n\t-f foreground");
				fprintf(stderr, "\n\t-g grab button if root window is selected");
				fprintf(stderr, "\n\t-u set up button (default: %d)", BTN_UP);
				fprintf(stderr, "\n\t-v verbose\n");
				return 1;
		}
	}

	if (!foreground)
	{
		/* daemonize */
		if ((pid = fork ()) == -1)
		{
			if (verbose)
				fprintf (stderr, "Unable to fork process: %s\n", strerror (errno));
			return 1;
		}
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
		(a_cur_desktop = XInternAtom(display, CUR_DESKTOP, True)) == None)
	{
		if (verbose)
			fprintf (stderr, "Unable to get atoms: %s, %s\n", NB_DESKTOP, CUR_DESKTOP);
		return 1;
	}

	/* search window(s) */
	if (argc > optind)
	{
		wins = (Window *) calloc (argc - optind, sizeof (Window));
		win = wins;
		for (i=optind; i<argc; i++)
		{
			if (verbose)
				fprintf (stderr, "Search for windows: %s\n", argv[i]);
			if (!strcmp (argv[i], "root"))
				*win = root;
			else
			{
				j=0;
				while (!(*win = window_by (display, root, argv[i], by_name)) &&
					j++<TRIES)
					sleep (TRY_SLEEP);
			}
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
		if (verbose)
			fprintf (stderr, "No windows found\n");
		free (wins);
		XCloseDisplay (display);
		return 2;
	}

	/* grab actions */
	for (win=wins; *win!=0; win++)
	{
		if (*win==root && !grab)
			XSelectInput(display, *win, StructureNotifyMask | ButtonPressMask);
		else
		{
			XSelectInput(display, *win, StructureNotifyMask);
			grab_btn (display, *win, btn_up);
			grab_btn (display, *win, btn_down);
		}
	}
	/* prepare the switch desktop event */
	xes.type = ClientMessage;
	xes.xclient.type = ClientMessage; 
	xes.xclient.display = display;
	xes.xclient.window = root;
	xes.xclient.message_type = a_cur_desktop;
	xes.xclient.format = 32;
	xes.xclient.data.l[1] = 2L;	/* timestamp */

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
		nb_desktop = get_win_prop (display, root, a_nb_desktop);
		cur_desktop = get_win_prop (display, root, a_cur_desktop);
		if (xeg.xbutton.button==btn_up)
		{
			if (--cur_desktop<0) cur_desktop = nb_desktop - 1;
		}
		else if (xeg.xbutton.button==btn_down)
		{
			if (++cur_desktop==nb_desktop) cur_desktop = 0;
		}
		else 
			continue;
		xes.xclient.data.l[0] = cur_desktop;
		/* switch desktop */
		XSendEvent(display, root, False, 
				 SubstructureNotifyMask | SubstructureRedirectMask,
				 &xes);
		XSync (display, False);
	}

	/* ungrab actions */
	for (win=wins; *win!=0; win++)
	{
		if (*win!=root || grab)
		{
			ungrab_btn (display, *win, btn_up);
			ungrab_btn (display, *win, btn_down);
		}
	}

	free (wins);
	XCloseDisplay (display);

	return 0;
}
