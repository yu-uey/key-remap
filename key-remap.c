#include <asm-generic/errno-base.h>
#include <fcntl.h>
#include <linux/input-event-codes.h>
#include <stdio.h>
#include <libevdev/libevdev.h>
#include <libevdev/libevdev-uinput.h>
#include <string.h>

// mod-tap state
char key_state = 0;
// track keys that are pressed after a mod-tap is pressed
unsigned int tracked_key[10];
unsigned int tracked_keys = 0;

void handle_key(const struct libevdev *dev, const struct libevdev_uinput *uidev,
                unsigned int code, int value) {
  if (code == KEY_D) {
    if (key_state == 0 && value == 1) {
      // key pressed while in "released" state -> enter "pressed" state
      key_state = 1;
      tracked_keys = 0;
    } else if (key_state == 1 && value == 0) {
      // key released from "pressed" -> tap key
      key_state = 0;
      libevdev_uinput_write_event(uidev, EV_KEY, KEY_D, 1);
      libevdev_uinput_write_event(uidev, EV_KEY, KEY_D, 0);

      // press tracked keys in order
      for (int i = 0; i < tracked_keys; i++) {
	libevdev_uinput_write_event(uidev, EV_KEY, tracked_key[i], 1);
      }
    } else if (key_state == 2 && value == 0) {
      // key released from "holding" -> release mod
      key_state = 0;
      libevdev_uinput_write_event(uidev, EV_KEY, KEY_LEFTCTRL, 0);
    }
  } else if (key_state == 1) {
    // "pressed" state waits for other key release
    if (value == 1) {
      // track other keys pressed while mod-tap is in "pressed" state
      tracked_key[tracked_keys] = code;
      if (tracked_keys < 9) tracked_keys++;
    } else if (value == 0) {
      // check if released key is tracked (pressed after mod-tap)
      char tracked = 0;
      for (int i = 0; i < tracked_keys; i++) {
	if (code == tracked_key[i]) {
	  tracked = 1;
	  break;
	}
      }
      if (tracked) {
	// "pressing" state -> start "holding" mod
	key_state = 2;
        libevdev_uinput_write_event(uidev, EV_KEY, KEY_LEFTCTRL, 1);
        for (int i = 0; i < tracked_keys; i++) {
	  libevdev_uinput_write_event(uidev, EV_KEY, tracked_key[i], 1);
        }
      }
      // always pass release event
      libevdev_uinput_write_event(uidev, EV_KEY, code, 0);
    }
  } else {
    // otherwise, pass the event through
    libevdev_uinput_write_event(uidev, EV_KEY, code, value);
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
	handle_key(dev, uidev, ev.code, ev.value);
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
