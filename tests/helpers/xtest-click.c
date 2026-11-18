#include <X11/Xlib.h>
#include <X11/extensions/XTest.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static void usage(const char *program)
{
  fprintf(stderr, "Usage: %s DISPLAY X Y\n", program);
  fprintf(stderr, "Sends a left click to the given X11 display and position.\n");
}

int main(int argc, char **argv)
{
  if (argc != 4)
  {
    usage(argv[0]);
    return 2;
  }

  Display *display = XOpenDisplay(argv[1]);
  if (!display)
  {
    fprintf(stderr, "Unable to open display %s\n", argv[1]);
    return 3;
  }

  const int x = atoi(argv[2]);
  const int y = atoi(argv[3]);

  XTestFakeMotionEvent(display, -1, x, y, 0);
  XFlush(display);
  usleep(100000);
  XTestFakeButtonEvent(display, 1, True, 0);
  XTestFakeButtonEvent(display, 1, False, 0);
  XFlush(display);
  usleep(100000);

  XCloseDisplay(display);
  return 0;
}
