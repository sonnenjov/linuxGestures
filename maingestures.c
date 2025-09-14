#include <libinput.h>
#include <libudev.h>
#include <fcntl.h>
#include <unistd.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
double total_dx = 0;
double total_dy = 0;
int current_finger_count = 0;
int initial_volume = -1;
static struct timespec last_set_time = {0, 0};

int get_volume() {
    FILE *fp = popen("pactl get-sink-volume @DEFAULT_SINK@ | grep -oP '\\d+%' | head -1", "r");
    if (!fp) return 50; 

    char buf[10];
    if (fgets(buf, sizeof(buf), fp) != NULL) {
        pclose(fp);
        return atoi(buf);
    }
    pclose(fp);
    return 50; 
}

void set_volume(int vol) {
    if (vol < 0) vol = 0;
    if (vol > 100) vol = 100;

    char cmd[128];
    snprintf(cmd, sizeof(cmd), "pactl set-sink-volume @DEFAULT_SINK@ %d%%", vol);
    system(cmd);
}

static int open_restricted(const char *path, int flags, void *user_data) {
    int fd = open(path, flags);
    if (fd < 0) {
        perror("Failed to open device");
    }
    return fd;
}

static void close_restricted(int fd, void *user_data) {
    close(fd);
}

const struct libinput_interface interface = {
    .open_restricted = open_restricted,
    .close_restricted = close_restricted,
};

void handle_gesture_event(struct libinput_event_gesture *gesture, enum libinput_event_type type) {


    static double accumulated_dy = 0;
    static double accumulated_dx = 0;
    static int initial_volume = 50;  
     struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    long elapsed_ms = (now.tv_sec - last_set_time.tv_sec) * 1000
                    + (now.tv_nsec - last_set_time.tv_nsec) / 1000000;
    int current_finger_count = libinput_event_gesture_get_finger_count(gesture);
    double dy = libinput_event_gesture_get_dy(gesture);
    double dx = libinput_event_gesture_get_dx(gesture);

    if (type == LIBINPUT_EVENT_GESTURE_SWIPE_BEGIN && current_finger_count == 4) {
        accumulated_dy = 0;
        initial_volume = get_volume(); 
        printf("Swipe begin, initial volume: %d%%\n", initial_volume);
    }
    else if (type == LIBINPUT_EVENT_GESTURE_SWIPE_UPDATE && current_finger_count == 4) {
        accumulated_dy += dy;

        double sensitivity = 400.0; 
        int volume_change = (int)(-accumulated_dy * 100.0 / sensitivity);
        int new_volume = initial_volume + volume_change;

        if (new_volume < 0) new_volume = 0;
        if (new_volume > 100) new_volume = 100;


        struct timespec now;
        clock_gettime(CLOCK_MONOTONIC, &now);

        static struct timespec last_set_time = {0, 0};
        long elapsed_ms = (now.tv_sec - last_set_time.tv_sec) * 1000
                          + (now.tv_nsec - last_set_time.tv_nsec) / 1000000;

        if (elapsed_ms > 50) {  
            set_volume(new_volume);
            last_set_time = now;
            initial_volume = new_volume;
            accumulated_dy = 0;
        }
    }

    else if (type == LIBINPUT_EVENT_GESTURE_SWIPE_END &&        current_finger_count == 4) {
        printf("Swipe ended\n");
        accumulated_dy = 0;
        initial_volume = -1;

    }
    
    else if (type == LIBINPUT_EVENT_GESTURE_SWIPE_BEGIN && current_finger_count == 3) {
            accumulated_dy=0;
            accumulated_dx=0;

    } else if (type == LIBINPUT_EVENT_GESTURE_SWIPE_UPDATE &&              current_finger_count == 3) {
        accumulated_dx+=dx;
        accumulated_dy+=dy;    
        struct timespec now;
            clock_gettime(CLOCK_MONOTONIC, &now);

            static struct timespec last_set_time = {0, 0};
            long elapsed_ms = (now.tv_sec - last_set_time.tv_sec) * 1000
                          + (now.tv_nsec - last_set_time.tv_nsec) / 1000000;
        if (elapsed_ms > 50) {
        last_set_time = now;

        if (abs(accumulated_dx) > abs(accumulated_dy)) {
            if (accumulated_dx > 0)
                system("xdotool key Tab");
            else
                system("xdotool key Shift+Tab");
        } else {
            if (accumulated_dy > 0) {
                system("xdotool key Escape");
                system("xdotool keyup Alt_L");
            } else
                system("xdotool keydown Alt_L key Tab");
        }

        accumulated_dx = 0;
        accumulated_dy = 0;
    }
                                    
    } else if (type == LIBINPUT_EVENT_GESTURE_SWIPE_END && current_finger_count == 3) {
                                    accumulated_dy = 0;
                                    accumulated_dx = 0;
    }
}
void handle_event(struct libinput_event *event) {
    enum libinput_event_type type = libinput_event_get_type(event);

    if (type == LIBINPUT_EVENT_GESTURE_SWIPE_BEGIN ||
        type == LIBINPUT_EVENT_GESTURE_SWIPE_UPDATE ||
        type == LIBINPUT_EVENT_GESTURE_SWIPE_END) {

        struct libinput_event_gesture *gesture = libinput_event_get_gesture_event(event);
        int fingers = libinput_event_gesture_get_finger_count(gesture);

        handle_gesture_event(gesture, type);
    }

    libinput_event_destroy(event);
}

int main() {
    struct udev *udev = udev_new();
    if (!udev) {
        fprintf(stderr, "Failed to initialize udev\n");
        return 1;
    }

    struct libinput *li = libinput_udev_create_context(&interface, NULL, udev);
    if (!li) {
        fprintf(stderr, "Failed to create libinput context\n");
        return 1;
    }

    if (libinput_udev_assign_seat(li, "seat0") != 0) {
        fprintf(stderr, "Failed to assign seat\n");
        return 1;
    }

    while (1) {
        libinput_dispatch(li);

        struct libinput_event *event;
        while ((event = libinput_get_event(li)) != NULL) {
            handle_event(event);
        }

        struct pollfd fds[] = {
            { libinput_get_fd(li), POLLIN, 0 }
        };
        poll(fds, 1, -1);
    }

    libinput_unref(li);
    udev_unref(udev);
    return 0;
}
