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


int debug;

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


int set_mtu(int mtu) {

  /*
  struct ifreq ifr; 
  int sock;
  
  if((sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
    return -1;
  }
  
  
  ifr.ifr_addr.sa_family = AF_INET;
  strncpy(ifr.ifr_name, iap->ifa_name, sizeof(ifr.ifr_name));

  ifr.ifr_mtu = mtu; 

  ioctl(s, SIOCSIFMTU, (caddr_t)&ifr);

  close(sock);
  */
}

// baud is specified using macros
// e.g. 9600 is B9600
int open_serial(char* dev, speed_t baud) {

  int fd;

  fd = open(dev, O_RDWR); /* connect to port */
  if(fd < 0) {
    fprintf(stderr, "Failed to open serial device %s: %s", dev, strerror(errno));
    return fd;
  }
  
  struct termios settings;
  if(tcgetattr(fd, &settings) < 0) {
    fprintf(stderr, "Failed to get serial device %s attributes: %s", dev, strerror(errno));
    return -1;
  }
  
  cfsetospeed(&settings, baud); /* baud rate */
  settings.c_cflag &= ~PARENB; /* no parity */
  settings.c_cflag &= ~CSTOPB; /* 1 stop bit */
  settings.c_cflag &= ~CSIZE;
  settings.c_cflag |= CS8 | CLOCAL; /* 8 bits */
  settings.c_lflag = ICANON; /* canonical mode */
  settings.c_oflag &= ~OPOST; /* raw output */
  
  if(tcsetattr(fd, TCSANOW, &settings) < 0) {
    fprintf(stderr, "Failed to set serial device %s attributes: %s", dev, strerror(errno));
    return -1;
  }
  
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

    // handle incoming data on serial device
    if(FD_ISSET(fds, &fdset)) {
      if(debug) {
        printf("Got data on serial port\n");
      }
      ret = rn2903_receive(fds, fdi);
      if(ret < 0) {
        return ret;
      }
    }

    // handle incoming data on network interface
    if(FD_ISSET(fdi, &fdset)) {

      // TODO receive data from fdi

      //      ret = rn2903_transmit(fds, fdi, data);
      if(ret < 0) {
        return ret;
      }
    }
  }
}


void usage(FILE* out, char* name) {
  fprintf(out, "Usage: %s\n", name);
}

void ping_report(int ret) {
  if(ret < 0) {
    printf("Got invalid response from RN2903\n");
  } else {
    printf("RN2903 is connected and responsive!\n");
  }
}

int main(int argc, char* argv[]) {
  int opt;

  char serial_dev[] = "/dev/ttyUSB0";
  speed_t serial_speed = B57600;
  char iface_name[IFNAMSIZ] = "lora";

  int ret;
  int fds; // serial fd
  int fdi; // interface fd

  int ping = 0;

  debug = 0;

  while((opt = getopt(argc, argv, "pd")) > 0) {
    switch(opt) {
      case 'p':
        ping = 1;
        break;
      case 'd':
        debug = 1;
        break;
      default:
        usage(stderr, argv[0]);
        return 1;
    }
  }

  argv += optind;
  argc -= optind;

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

  if(ping) {
    if(debug) {
      printf("Preparing to ping\n");
    }
    rn2903_check(fds, ping_report);
  }

  ret = event_loop(fds, fdi);
  if(ret < 0) {
    return ret;
  }

  return 0;
}
