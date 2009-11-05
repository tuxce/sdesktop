/*
 * 15/05/2009 -- Alain Ducasse
 *
 * Switch workspace with your mousewheel for metacity window manager.
 * 
 * Fr : Change de bureau avec la molette de la souris pour le gestionaire
 *      de fenetre metacity (a la mode de compiz-fusion).
 *
 * Compile : 
 *   gcc switch_workspace_metacity.c -o switch_workspace_metacity -lX11
 *
 * (Required library : libX11-dev)
 *
 * To stop : CTRL+C or 'killall switch_workspace_metacity'
 *
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <signal.h>
#include <unistd.h>

#include <X11/Xlib.h>
#include <X11/Xatom.h>

#define LOG_FILE       /* log file in /var/tmp/ for debug */

static int   G_ok = 1;

/*--------------------------------------------------------------------------*/

unsigned int get_nb_desktop(Display *disp, Window root)
{
  Atom           a;
  Atom           xa_ret_type;
  int            ret_format;
  unsigned long  ret_nitems;
  unsigned long  ret_bytes_after;
  unsigned char  *ret_prop;
  unsigned int   nb_desktop;

  a = XInternAtom(disp, "_NET_NUMBER_OF_DESKTOPS", False);
  if (XGetWindowProperty(disp, root, a, 0,
        1, False, XA_CARDINAL, &xa_ret_type, &ret_format,
        &ret_nitems, &ret_bytes_after, &ret_prop) != Success)
  {
    fprintf(stderr,"Cannot get _NET_NUMBER_OF_DESKTOPS property\n");
    return -1;
  }

  if (xa_ret_type != XA_CARDINAL)
  {
    fprintf(stderr,"Invalid type of _NET_NUMBER_OF_DESKTOPS property\n");
    XFree(ret_prop);
    return -1;
  }
  
  assert(ret_nitems == 1);
  assert(ret_format == 32);

  memcpy((unsigned char *) &nb_desktop, ret_prop, 4);

  XFree(ret_prop);

  return nb_desktop;
}

unsigned int get_cur_desktop(Display *disp, Window root)
{
  Atom           a;
  Atom           xa_ret_type;
  int            ret_format;
  unsigned long  ret_nitems;
  unsigned long  ret_bytes_after;
  unsigned char  *ret_prop;
  unsigned int   cur_desktop;

  a = XInternAtom(disp, "_NET_CURRENT_DESKTOP", False);
  if (XGetWindowProperty(disp, root, a, 0,
        1, False, XA_CARDINAL, &xa_ret_type, &ret_format,
        &ret_nitems, &ret_bytes_after, &ret_prop) != Success)
  {
    fprintf(stderr,"Cannot get _NET_CURRENT_DESKTOP property\n");
    return -1;
  }

  if (xa_ret_type != XA_CARDINAL)
  {
    fprintf(stderr,"Invalid type of _NET_CURRENT_DESKTOP property\n");
    XFree(ret_prop);
    return -1;
  }
  
  assert(ret_nitems == 1);
  assert(ret_format == 32);

  memcpy((unsigned char *) &cur_desktop, ret_prop, 4);

  XFree(ret_prop);

  return cur_desktop;
}

void set_cur_desktop(Display *disp, Window root, unsigned int cur_desktop)
{
  Atom           a;
  XEvent         xevent;

  a = XInternAtom(disp, "_NET_CURRENT_DESKTOP", False);

  xevent.type                 = ClientMessage;
  xevent.xclient.type         = ClientMessage; 
  xevent.xclient.display      = disp;
  xevent.xclient.window       = root;
  xevent.xclient.message_type = a;
  xevent.xclient.format       = 32;
  xevent.xclient.data.l[0]    = cur_desktop;
  xevent.xclient.data.l[1]    = CurrentTime;
  xevent.xclient.data.l[2]    = 0;
  xevent.xclient.data.l[3]    = 0;
  xevent.xclient.data.l[4]    = 0;

  XSendEvent(disp, root, False, 
             SubstructureNotifyMask | SubstructureRedirectMask,
             &xevent);
}

void grab_buttons(Display *disp, Window win)
{
  int i;

  for(i = 4; i <= 5; i++)  /* mouse wheel : UP (4) & DOWN (5) */
  {
    XGrabButton(disp,i,AnyModifier,win,
                False, ButtonPressMask,
                GrabModeAsync, GrabModeAsync,
                None,None);
  }
}

void ungrab_buttons(Display *disp, Window root, Window win)
{
  int i;

  XSync(disp,False);

  for(i = 4; i <= 5; i++)
  {
    XUngrabButton(disp,i,AnyModifier,win);
  }
  XUngrabButton(disp, AnyButton, AnyModifier, root);

  XSync(disp,False);
}

Window find_window(Display *disp, Window root, char *wname)
{
  Window   r;
  Window   win;
  Window   *kids;
  unsigned i,nkids;
  char     *title;

  XQueryTree(disp,root,&r,&win,&kids,&nkids);
  win = root;

  for (i = 0; i < nkids; i++)
  {
    XFetchName(disp,kids[i],&title);
    if (title && strcmp(title,wname) == 0)
    {
      win = kids[i];
      XFree(title);
      break;
    }
    if (title) 
      XFree(title);
  }
  XFree(kids);

  return win;
}

void handler_signal(int param)
{
  /* CTRL-C or KILL */
  G_ok = 0;
}

void switch_workspace_with_mousewheel(void)
{
  Display        *disp;
  XEvent         xe;
  Window         root,desktop;
  unsigned int   cur_desktop;
  unsigned int   nb_desktop;
  int            wait_desktop;
#ifdef LOG_FILE
  FILE           *f;

  f = fopen("/var/tmp/switch_workspace_metacity.log","wb");
  assert(f);
#endif

  disp = XOpenDisplay(NULL);

  if (!disp)
  {
#ifdef LOG_FILE
    fprintf(f,"Erreur XOpenDisplay()\n");
#endif
    exit(-1);
  }

  root = DefaultRootWindow(disp);

  wait_desktop = 0;
  desktop      = root;
  while ((wait_desktop < 60) && (desktop == root))
  {
    desktop = find_window(disp, root, "x-nautilus-desktop");
    sleep(1);
    wait_desktop++;
  }

#ifdef LOG_FILE
  fprintf(f,"Attente desktop : %d s\n",wait_desktop);
#endif

  if (desktop != root)
  {
    fprintf(stdout,
        "\n>>> Switch workspace with your mousewheel for metacity\n\n");

    nb_desktop = get_nb_desktop(disp,root);
 
    cur_desktop = get_cur_desktop(disp,root);

#ifdef LOG_FILE
    fprintf(f,"nb_desktop = %d, cur_desktop = %d\n",nb_desktop,cur_desktop);
#endif
 
    XAllowEvents(disp,AsyncBoth,CurrentTime);
 
    grab_buttons(disp,desktop);

    signal(SIGINT,handler_signal);
    signal(SIGTERM,handler_signal);

#ifdef LOG_FILE
    fprintf(f,"Attent la molette ...\n");
    fflush(f);
#endif
 
    while (1)
    {

      // while (G_ok && (XPending(disp) == 0))
      //   usleep(100);

      if (!G_ok)
        break;

      XNextEvent(disp,&xe);

      if(xe.type == ButtonPress)
      {
#ifdef LOG_FILE
        fprintf(f,"    %s\n",xe.xbutton.button == 5 ? "DOWN" : "UP");
        fflush(f);
#endif

        if (xe.xbutton.button == 5)
        {
          cur_desktop = (cur_desktop + 1) % nb_desktop;
        }
        else
        {
          if (cur_desktop)
            cur_desktop--;
          else
            cur_desktop = nb_desktop - 1;
        }
      
        set_cur_desktop(disp, root, cur_desktop);
      }
    }

    ungrab_buttons(disp,root,desktop);
  }
#ifdef LOG_FILE
  else
    fprintf(f,"Desktop non trouve !!!\n");
#endif

#ifdef LOG_FILE
  fprintf(f,"Exit de switch_workspace_with_mousewheel()\n");

  fclose(f);
#endif

  XCloseDisplay(disp);
}


int main()
{
  switch_workspace_with_mousewheel();

  return 0;
}
