#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <libevdev/libevdev.h>
#include <libevdev/libevdev-uinput.h>
#include <string.h>

// track keys that are pressed after a mod-tap is pressed
unsigned int tracked_key[10];
unsigned int tracked_keys = 0;

typedef struct {
  const unsigned int tap;
  const unsigned int mod;
  char active;
  long tv_sec;
  long tv_usec;
} modtap;

modtap modtaps[] = {
    {KEY_D, KEY_LEFTCTRL},
    {KEY_K, KEY_LEFTCTRL},
    {KEY_S, KEY_LEFTALT},
    {KEY_L, KEY_LEFTALT},
    {KEY_A, KEY_LEFTMETA},
    {KEY_SEMICOLON, KEY_LEFTMETA},
};

const int n_modtaps = sizeof(modtaps) / sizeof(modtap);
modtap *get_modtap(unsigned int code) {
  for (int i = 0; i < n_modtaps; i++) {
    if (modtaps[i].tap == code) {
      return &modtaps[i];
    }
  }
  return NULL;
}

void handle_key(const struct libevdev_uinput *uidev, const struct input_event *ev) {
  modtap *mt = get_modtap(ev->code);
  if (tracked_keys < 1 && mt == NULL) {
    // Not tracked and not mod-tap => pass through
    libevdev_uinput_write_event(uidev, EV_KEY, ev->code, ev->value);
  } else if (ev->value == 1) {
    // Start tracking on mod-tap, or add to tracking queue.
    // (This branch is only reached if tracking is active or key is mod-tap)
    tracked_key[tracked_keys++] = ev->code;
    if (mt != NULL) {
      mt->tv_sec = ev->time.tv_sec;
      mt->tv_usec = ev->time.tv_usec;
    }
  } else if (ev->value == 0) {
    if (tracked_keys > 0) {
      // Any key release during tracking => collapse queue
      // If released key was not tracked, all keys are pressed as taps
      // Collapsing on non-tracked keys makes uppercase homerow letters easier to type
      char tracked = 0;
      for (int i = tracked_keys - 1; i > 0; i--) {
        if (ev->code == tracked_key[i]) {
          tracked = i;
          break;
        }
      }
      // press all preceding tracked keys with mod-taps as mod
      for (int i = 0; i < tracked; i++) {
        unsigned int key = tracked_key[i];
        modtap *mt = get_modtap(key);
        if (mt != NULL) {
          key = mt->mod;
          mt->active = 1;
        }
        libevdev_uinput_write_event(uidev, EV_KEY, key, 1);
      }
      // Check tap-term and send as mod if needed
      if (mt != NULL && !mt->active && mt->tv_sec) {
        long dt = (ev->time.tv_sec - mt->tv_sec) * 1e6 +
                  (ev->time.tv_usec - mt->tv_usec);
	if (dt > 250 * 1e3) {
          mt->active = 1;
	  libevdev_uinput_write_event(uidev, EV_KEY, mt->mod, 1);
	  tracked++; // if sending here, skip this press in the next part
	}
      }
      // Press released key and all proceeding keys as is
      for (int i = tracked; i < tracked_keys; i++) {
        libevdev_uinput_write_event(uidev, EV_KEY, tracked_key[i], 1);
      }
      tracked_keys = 0;
    }
    // Pass forward key release event
    if (mt != NULL) {
      // Release of mod-tap key => check state and tap-term
      libevdev_uinput_write_event(uidev, EV_KEY, (mt->active ? mt->mod : mt->tap), 0);
      mt->active = 0;
      mt->tv_sec = 0;
      mt->tv_usec = 0;
    } else {
      // Release of any other key
      libevdev_uinput_write_event(uidev, EV_KEY, ev->code, 0);
    }
  }
}

int start_uinput_grab(struct libevdev *dev, struct libevdev_uinput **uidev) {
  int rc;
  // Create uinput device
  rc = libevdev_uinput_create_from_device(dev, LIBEVDEV_UINPUT_OPEN_MANAGED,
                                          uidev);
  if (rc < 0) {
    fprintf(stderr, "Failed to init uinput device (%s)\n", strerror(-rc));
    return rc;
  }
  // Grab evdev device
  rc = libevdev_grab(dev, LIBEVDEV_GRAB);
  if (rc < 0) {
    fprintf(stderr, "Failed to grab evdev device (%s)\n", strerror(-rc));
    return rc;
  }
  return 0;
}

int stop_uinput_grab(struct libevdev *dev, struct libevdev_uinput *uidev) {
  int rc;
  // Ungrab evdev device
  rc = libevdev_grab(dev, LIBEVDEV_UNGRAB);
  if (rc < 0) {
    fprintf(stderr, "Failed to ungrab evdev device (%s)\n", strerror(-rc));
    return rc;
  }
  // Destroy uinput device
  libevdev_uinput_destroy(uidev);
  return 0;
}

int main(int argc, char **argv) {
  struct libevdev *dev = NULL;
  struct libevdev_uinput *uidev = NULL;
  const char *file;
  int fd;
  int rc = 1;
  char grab_active = 1;

  // Open the file
  if (argc < 2) {
    goto out;
  }
  file = argv[1];
  fd = open(file, O_RDONLY);
  if (fd < 0) {
    perror("Failed to open device");
    goto out;
  }

  // Init and grab libevdev device
  rc = libevdev_new_from_fd(fd, &dev);
  if (rc < 0) {
    fprintf(stderr, "Failed to init libevdev (%s)\n", strerror(-rc));
    goto out;
  }
  printf("Input device name: \"%s\"\n", libevdev_get_name(dev));
  printf("Input device ID: bus %#x vendor %#x product %#x\n",
         libevdev_get_id_bustype(dev), libevdev_get_id_vendor(dev),
         libevdev_get_id_product(dev));

  rc = start_uinput_grab(dev, &uidev);
  if (rc) goto out;

  do {
    struct input_event ev;
    rc = libevdev_next_event(
        dev, LIBEVDEV_READ_FLAG_NORMAL | LIBEVDEV_READ_FLAG_BLOCKING, &ev);

    if (rc == LIBEVDEV_READ_STATUS_SUCCESS) {
      // Only relay to uinput while grab is active
      // Toggle grab activation with special key release
      if (ev.type == EV_KEY && ev.code == KEY_F9 && ev.value == 0) {
        if (grab_active) {
          rc = stop_uinput_grab(dev, uidev);
          if (rc) goto out;
	  grab_active = 0;
        } else {
          rc = start_uinput_grab(dev, &uidev);
	  if (rc) goto out;
	  grab_active = 1;
        }
      } else if (grab_active) {
	// Handle key events, pass through non-key events
	if (ev.type == EV_KEY) {
	  handle_key(uidev, &ev);
	} else {
	  libevdev_uinput_write_event(uidev, ev.type, ev.code, ev.value);
	}
      }
    }
  } while (rc == LIBEVDEV_READ_STATUS_SYNC || rc == LIBEVDEV_READ_STATUS_SUCCESS || rc == -EAGAIN);

out:
  libevdev_uinput_destroy(uidev);
  libevdev_free(dev);
  return rc;
}
