#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <string.h>


#define NB_DESKTOP 	"_NET_NUMBER_OF_DESKTOPS"
#define CUR_DESKTOP	"_NET_CURRENT_DESKTOP"

#define BTN_UP		Button4	/* mouse wheel up */
#define BTN_DOWN	Button5	/* mouse wheel down */

/* default window for handling mouse wheel */
#define WM_DESKTOP "x-nautilus-desktop"	

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
		Xfree (data);
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
	unsigned int i;
	int by_name=0;
	pid_t pid;

	/* daemonize */
	if ((pid = fork ()) == -1)
	{
		return 1;
	}
	if (pid)
	{
		return 0;
	}

	/* handle signals */
	signal(SIGINT,&end_program);
	signal(SIGTERM,&end_program);

	display = XOpenDisplay(NULL);
	root = DefaultRootWindow(display);

	if ((a_nb_desktop = XInternAtom(display, NB_DESKTOP, True)) == None ||
		(a_cur_desktop = XInternAtom(display, CUR_DESKTOP, True)) == None)
	{
		return 2;
	}

	/* search window(s) */
	if (argc > 1)
	{
		wins = (Window *) calloc (argc, sizeof (Window));
		win = wins;
		for (i=1; i<argc; i++)
		{
			if (!strcmp (argv[i], "-n") || !strcmp(argv[i], "-c"))
			{
				by_name = (argv[i][1] == 'n') ? 1 : 0;
				continue;
			}
			*win = window_by (display, root, argv[i], by_name);
			if (*win) win++;
		}
		*win = 0;
	}
	else
	{
		wins = (Window *) calloc (2, sizeof (Window));
		wins[0] = window_by (display, root, WM_DESKTOP, 1);
		wins[1] = 0;
	}
	if (!wins[0])
	{
		return 3;
	}

	/* grab actions */
	for (win=wins; *win!=0; win++)
	{
		grab_btn (display, *win, BTN_UP);
		grab_btn (display, *win, BTN_DOWN);
	}
	/* prepare the switch desktop event */
	xes.type = ClientMessage;
	xes.xclient.type = ClientMessage; 
	xes.xclient.display = display;
	xes.xclient.window = root;
	xes.xclient.message_type = a_cur_desktop;
	xes.xclient.format = 32;

	i=10;	
	while (--i>0)
	{
		/* wait for an event */
		XNextEvent(display,&xeg);
		if (terminate)
			break;
		nb_desktop = get_win_prop (display, root, a_nb_desktop);
		cur_desktop = get_win_prop (display, root, a_cur_desktop);
		switch (xeg.xbutton.button)
		{
			case BTN_UP:
				if (--cur_desktop<0) cur_desktop = nb_desktop - 1;
				break;
			case BTN_DOWN:
				if (++cur_desktop==nb_desktop) cur_desktop = 0;
				break;
			default:
				continue;
		}
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
		ungrab_btn (display, *win, BTN_UP);
		ungrab_btn (display, *win, BTN_DOWN);
	}

	free (wins);
	XCloseDisplay (display);

	return 0;
}
