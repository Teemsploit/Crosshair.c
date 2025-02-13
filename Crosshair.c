#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/Xcomposite.h>
#include <X11/Xatom.h>
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <unistd.h>
#include <signal.h>
#include <libnotify/notify.h>

#define CROSSHAIR_SIZE 20

static Display *display;
static Window root, overlay;
static GC gc;
static int screen;
static int crosshair_color = 0xFF0000;
static int visible = 1;

void draw_crosshair() {
    XSetForeground(display, gc, crosshair_color);
    int center_x = DisplayWidth(display, screen) / 2;
    int center_y = DisplayHeight(display, screen) / 2;

    XClearWindow(display, overlay);
    XDrawLine(display, overlay, gc, center_x - CROSSHAIR_SIZE, center_y, center_x + CROSSHAIR_SIZE, center_y);
    XDrawLine(display, overlay, gc, center_x, center_y - CROSSHAIR_SIZE, center_x, center_y + CROSSHAIR_SIZE);
    XFlush(display);
}

void toggle_visibility() {
    visible = !visible;
    if (visible) {
        XMapRaised(display, overlay);
        notify_notification_show(notify_notification_new("Crosshair", "Enabled", NULL), NULL);
    } else {
        XUnmapWindow(display, overlay);
        notify_notification_show(notify_notification_new("Crosshair", "Disabled", NULL), NULL);
    }
}

void cycle_color() {
    if (crosshair_color == 0xFF0000)      crosshair_color = 0x00FF00;
    else if (crosshair_color == 0x00FF00) crosshair_color = 0x0000FF;
    else if (crosshair_color == 0x0000FF) crosshair_color = 0xFF0000;
    draw_crosshair();
    notify_notification_show(notify_notification_new("Crosshair", "Color Changed", NULL), NULL);
}

void *input_listener(void *arg) {
    system("xev -root | awk '/keycode/ {print $5}' > /tmp/crosshair_input &");
    FILE *fp;
    char key[10];
    while (1) {
        fp = fopen("/tmp/crosshair_input", "r");
        if (fp) {
            while (fgets(key, sizeof(key), fp)) {
                if (atoi(key) == 110) toggle_visibility();
                if (atoi(key) == 118) cycle_color();
            }
            fclose(fp);
        }
        usleep(200000);
    }
    return NULL;
}

void setup_overlay() {
    XSetWindowAttributes attrs;
    attrs.override_redirect = True;
    attrs.background_pixel = 0x000000;
    attrs.save_under = True;
    attrs.event_mask = ExposureMask;
    
    overlay = XCreateWindow(display, root, 0, 0, DisplayWidth(display, screen), DisplayHeight(display, screen), 0,
                            CopyFromParent, InputOutput, CopyFromParent, CWOverrideRedirect | CWBackPixel | CWSaveUnder, &attrs);
    
    Atom atom = XInternAtom(display, "_NET_WM_WINDOW_TYPE", False);
    Atom type = XInternAtom(display, "_NET_WM_WINDOW_TYPE_DOCK", False);
    XChangeProperty(display, overlay, atom, XA_ATOM, 32, PropModeReplace, (unsigned char *)&type, 1);
    
    XMapWindow(display, overlay);
    gc = XCreateGC(display, overlay, 0, NULL);
    draw_crosshair();
}

int main() {
    notify_init("Crosshair");
    display = XOpenDisplay(NULL);
    if (!display) {
        fprintf(stderr, "Failed to open X display\n");
        return 1;
    }
    screen = DefaultScreen(display);
    root = RootWindow(display, screen);
    
    setup_overlay();
    
    pthread_t input_thread;
    pthread_create(&input_thread, NULL, input_listener, NULL);
    
    while (1) {
        sleep(1);
        if (visible) draw_crosshair();
    }
    
    notify_uninit();
    XCloseDisplay(display);
    return 0;
}
