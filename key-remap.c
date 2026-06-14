#include <asm-generic/errno-base.h>
#include <fcntl.h>
#include <stdio.h>
#include <libevdev/libevdev.h>
#include <libevdev/libevdev-uinput.h>
#include <string.h>

void handle_ev(const struct libevdev *dev, const struct libevdev_uinput *uidev,
               const struct input_event *ev) {
  printf("Event: type %d (%s), code %d (%s), value %d\n", ev->type,
         libevdev_event_type_get_name(ev->type), ev->code,
         libevdev_event_code_get_name(ev->type, ev->code), ev->value);
  libevdev_uinput_write_event(uidev, ev->type, ev->code, ev->value);
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
      handle_ev(dev, uidev, &ev);
    }
  } while (rc == LIBEVDEV_READ_STATUS_SYNC || rc == LIBEVDEV_READ_STATUS_SUCCESS || rc == -EAGAIN);

out:
  libevdev_uinput_destroy(uidev);
  libevdev_free(dev);
  return rc;
}
