#include <X11/Xlib.h>
#include <X11/Xatom.h>


#define NB_DESKTOP 	"_NET_NUMBER_OF_DESKTOPS"
#define CUR_DESKTOP	"_NET_CURRENT_DESKTOP"

#define BTN_UP		4	/* mouse wheel up */
#define BTN_DOWN	5	/* mouse wheel down */

#define WM_DESKTOP "x-nautilus-desktop"


long get_win_prop (Display *d, Window w, Atom a)
{
	long ret=-1;
	Atom a_type;
	int a_format;
	unsigned long nitems, bytes;
	unsigned char *data;

	if (XGetWindowProperty(d, w, a, 0, 1, False, XA_CARDINAL,
		&a_type, &a_format, &nitems, &bytes, &data) == Success)
		if (nitems != 0 && a_format == 32)
		{
			ret = *((unsigned long *) data);
			Xfree (data);
		}
	return ret;
}

void grab_btn (Display *d, Window w, unsigned int btn)
{
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
	XUngrabButton (d, btn, AnyModifier, w);
}


/* fonction provenant de xprop/dsimple.c */
Window window_by_name (Display *d, Window r, const char *name)
{
	Window *children, dummy;
	unsigned int nchildren;
	int i;
	Window w=0;
	char *window_name;

	if (XFetchName(d, r, &window_name) && !strcmp(window_name, name))
	  return(r);

	if (!XQueryTree(d, r, &dummy, &dummy, &children, &nchildren))
	  return(0);

	for (i=0; i<nchildren; i++) {
		w = window_by_name (d, children[i], name);
		if (w)
		  break;
	}
	if (children) XFree ((char *)children);
	return(w);
}


int main ()
{
	Display *display;
	Window root;
	Window desktop;
	Atom a_nb_desktop;
	Atom a_cur_desktop;
	XEvent xes, xeg;
	long nb_desktop, cur_desktop;

	display = XOpenDisplay(NULL);
    root = DefaultRootWindow(display);
	if ((a_nb_desktop = XInternAtom(display, NB_DESKTOP, True)) == None ||
		(a_cur_desktop = XInternAtom(display, CUR_DESKTOP, True)) == None)
	{
		return 1;
	}


	desktop = window_by_name (display, root, WM_DESKTOP);
	if (!desktop)
	{
		return 2;
	}

	grab_btn (display, desktop, BTN_UP);
	grab_btn (display, desktop, BTN_DOWN);
	xes.type = ClientMessage;
	xes.xclient.type = ClientMessage; 
	xes.xclient.display = display;
	xes.xclient.window = root;
	xes.xclient.message_type = a_cur_desktop;
	xes.xclient.format = 32;

	while (1)
	{
		XNextEvent(display,&xeg);
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
		XSendEvent(display, root, False, 
				 SubstructureNotifyMask | SubstructureRedirectMask,
				 &xes);
		XSync (display, False);
	}



	grab_btn (display, root, BTN_UP);
	grab_btn (display, root, BTN_DOWN);

	return 0;
}
