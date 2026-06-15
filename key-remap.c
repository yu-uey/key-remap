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
  unsigned int tap;
  unsigned int mod;
  char active;
} modtap;

modtap modtaps[] = {
    {KEY_D, KEY_LEFTCTRL, 0},
    {KEY_K, KEY_LEFTCTRL, 0},
    {KEY_S, KEY_LEFTALT, 0},
    {KEY_L, KEY_LEFTALT, 0},
    {KEY_A, KEY_LEFTMETA, 0},
    {KEY_SEMICOLON, KEY_LEFTMETA, 0},
};

modtap *get_modtap(unsigned int code) {
  for (int i = 0; i < sizeof(modtaps) / sizeof(modtap); i++) {
    if (modtaps[i].tap == code) {
      return &modtaps[i];
    }
  }
  return NULL;
}

void handle_key(const struct libevdev_uinput *uidev, const unsigned int code,
                const int value) {
  if (tracked_keys < 1) {
    modtap *mt = get_modtap(code);
    if (mt != NULL) {
      // start tracking keys if mod-tap key is pressed
      if (value == 1) {
	tracked_keys = 0;
	tracked_key[tracked_keys++] = code;
      } else if (value == 0) {
	// check mod-tap state on release
        libevdev_uinput_write_event(uidev, EV_KEY,
                                    (mt->active ? mt->mod : mt->tap), 0);
	mt->active = 0;
      }
      // ignore mod-tap key's repeat events
    } else {
      // not mod-tap => pass key event through
      libevdev_uinput_write_event(uidev, EV_KEY, code, value);
    }
  } else {
    if (value == 1) {
      // track newly pressed keys
      tracked_key[tracked_keys++] = code;
    } else if (value == 0) {
      // collapse queue on release of any key
      // if released key was not tracked, all keys are pressed as taps
      // collapsing on non-tracked keys makes uppercase homerow letters easier to type
      char tracked = 0;
      for (int i = tracked_keys - 1; i > 0; i--) {
        if (code == tracked_key[i]) {
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
      // press all proceeding tracked keys as taps
      for (int i = tracked; i < tracked_keys; i++) {
        libevdev_uinput_write_event(uidev, EV_KEY, tracked_key[i], 1);
      }
      // then release original event
      modtap *mt = get_modtap(code);
      if (mt != NULL) {
        libevdev_uinput_write_event(uidev, EV_KEY,
                                    (mt->active ? mt->mod : mt->tap), 0);
        mt->active = 0;
      } else {
        libevdev_uinput_write_event(uidev, EV_KEY, code, 0);
      }
      tracked_keys = 0;
    }
    // ignore key repeats during tracking
  }
}

int main(int argc, char **argv) {
  struct libevdev *dev = NULL;
  struct libevdev_uinput *uidev = NULL;
  const char *file;
  int fd;
  int rc = 1;

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
  libevdev_grab(dev, LIBEVDEV_GRAB);

  // Create uinput device
  rc = libevdev_uinput_create_from_device(dev, LIBEVDEV_UINPUT_OPEN_MANAGED,
                                          &uidev);
  if (rc < 0) {
    fprintf(stderr, "Failed to init uinput device (%s)\n", strerror(-rc));
    goto out;
  }

  do {
    struct input_event ev;
    rc = libevdev_next_event(
        dev, LIBEVDEV_READ_FLAG_NORMAL | LIBEVDEV_READ_FLAG_BLOCKING, &ev);

    if (rc == LIBEVDEV_READ_STATUS_SUCCESS) {
      // Always pass non-key events 
      if (ev.type == EV_KEY) {
	handle_key(uidev, ev.code, ev.value);
      } else {
	libevdev_uinput_write_event(uidev, ev.type, ev.code, ev.value);
      }
    }
  } while (rc == LIBEVDEV_READ_STATUS_SYNC || rc == LIBEVDEV_READ_STATUS_SUCCESS || rc == -EAGAIN);

out:
  libevdev_uinput_destroy(uidev);
  libevdev_free(dev);
  return rc;
}
