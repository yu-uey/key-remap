#include <fcntl.h>
#include <stdio.h>
#include <libevdev/libevdev.h>
#include <string.h>

int main(int argc, char **argv) {
  struct libevdev *dev = NULL;
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

  // Create libevdev device
  rc = libevdev_new_from_fd(fd, &dev);
  if (rc < 0) {
    fprintf(stderr, "Failed to init libevdev (%s)\n", strerror(-rc));
    goto out;
  }

  printf("Input device name: \"%s\"\n", libevdev_get_name(dev));
  printf("Input device ID: bus %#x vendor %#x product %#x\n",
         libevdev_get_id_bustype(dev), libevdev_get_id_vendor(dev),
         libevdev_get_id_product(dev));

out:
  libevdev_free(dev);
  return rc;
}
