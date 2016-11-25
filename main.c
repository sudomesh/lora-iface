#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <stdarg.h>
#include <termios.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/time.h>
#include <linux/if.h>
#include <linux/if_tun.h>

// either call with a pre-allocated 
//   char dev[IFNAMSIZ]
// set to the preferred dev name and it will be replaced with the allotted name
// or call with a NULL pointer and a char* will be malloc'ed and returned
// with the allotted name
int create_tun(char* dev) {

  struct ifreq ifr;
  int fd;
  int ret;

  if((fd = open("/dev/net/tun", O_RDWR)) < 0 ) {
    perror("Error opening /dev/net/tun");
    return fd;
  }

  memset(&ifr, 0, sizeof(ifr));

  ifr.ifr_flags = IFF_TUN;

  if(dev) {
    strncpy(ifr.ifr_name, dev, IFNAMSIZ);
  } else {
    dev = malloc(IFNAMSIZ);
    if(!dev) {
      perror("Malloc failed");
      return -1;
    }
  }

  ret = ioctl(fd, TUNSETIFF, (void*) &ifr);
  if(ret < 0) {
    perror("Error during TUNSETIFF ioctl");
    close(fd);
    return ret;
  }

  strcpy(dev, ifr.ifr_name);

  return fd;
}


// baud is specified using macros
// e.g. 9600 is B9600
int open_serial(char* dev, speed_t baud) {

  int fd;

  fd = open(dev, O_RDWR); /* connect to port */
  
  struct termios settings;
  tcgetattr(fd, &settings);
  
  cfsetospeed(&settings, baud); /* baud rate */
  settings.c_cflag &= ~PARENB; /* no parity */
  settings.c_cflag &= ~CSTOPB; /* 1 stop bit */
  settings.c_cflag &= ~CSIZE;
  settings.c_cflag |= CS8 | CLOCAL; /* 8 bits */
  settings.c_lflag = ICANON; /* canonical mode */
  settings.c_oflag &= ~OPOST; /* raw output */
  
  tcsetattr(fd, TCSANOW, &settings); /* apply the settings */
  tcflush(fd, TCOFLUSH);
  
  return fd;
}

int close_serial(int fd) {
  close(fd);
}


int event_loop(int fds, int fdi) {
  int ret;
  int maxfd;
  fd_set fdset;

  while(1) {
    
    FD_ZERO(&fdset);

    FD_SET(fds, &fdset);
    FD_SET(fdi, &fdset);

    maxfd = (fds > fdi) ? fds : fdi;

    maxfd = add_uclients_to_fd_set(&fdset, maxfd);

    ret = select(maxfd + 1, &fdset, NULL, NULL, NULL);
    if(ret < 0){
      if(errno == EINTR) {
        continue;
      } else {
        perror("Error during select()");
        return -1;
      }
    }

    handle_uclient_connections(&fdset);

    // TODO actually read and write between sockets


  }
}




int main() {

  char serial_dev[] = "/dev/ttyS0";
  speed_t serial_speed = B9600;
  char iface_name[IFNAMSIZ] = "lora";

  int ret;
  int fds; // serial fd
  int fdi; // interface fd


  // TODO check if we are simply talking to an existing uclient
  // and call send_uclient_msg accordingly
  //ret = send_uclient_msg('i', NULL, 1);


  fds = open_serial(serial_dev, serial_speed);
  if(fds < 0) {
    return fds;
  }

  fdi = create_tun(iface_name);
  if(fdi < 0) {
    close(fds);
    return fdi;
  }

  open_ipc_socket();

  ret = event_loop(fds, fdi);
  if(ret < 0) {
    return ret;
  }

  return 0;
}
