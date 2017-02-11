#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <stdarg.h>
#include <termios.h>
#include <grp.h>
#include <pwd.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/types.h>
#include <sys/time.h>
#include <linux/if.h>
#include <linux/if_tun.h>

// group and user to run this program as
#define RUNAS_GROUP "juul"
#define RUNAS_USER "juul"

#define TX_QUEUE_LENGTH (20)
#define LORA_MTU (500)

int debug;

// drop root privileges
int drop_privs(char* group_name, char* user_name) {
  struct passwd *pwd;
  struct group *grp;
  gid_t group_id;
  uid_t user_id;

  if(getuid() != 0) { 
    return -1;
  }

  // DO NOT FREE grp (see man getgrnam)
  grp = getgrnam(group_name);
  if(grp == NULL) {
    perror("could not find group id from group name");
    return -1;
  }

  group_id = grp->gr_gid;
  grp = NULL;

  // DO NOT FREE pwd (see man getpwnam)
  pwd = getpwnam(user_name);
  if(pwd == NULL) {
    perror("could not find user id from user name");
    return -1;
  }

  user_id = pwd->pw_uid;
  pwd = NULL;

  if(setgid(group_id) != 0) {
    perror("unable to drop group privilege from root");
    return -1;
  }

  if(setuid(user_id) != 0) {
    perror("unable to drop user privilege from root");
    return -1;    
  }

  return 0;
}

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


int get_txqueuelen(char* ifname) {

  struct ifreq ifr; 
  int fd;
  int ret;
  
  if((fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
    return -1;
  }

  ifr.ifr_addr.sa_family = AF_INET;
  strncpy(ifr.ifr_name, ifname, IFNAMSIZ);

  ioctl(fd, SIOCGIFTXQLEN, (caddr_t) &ifr);
  if(ret < 0) {
    perror("Error during SIOCGIFTXQLEN ioctl (get txqueuelen)");
    close(fd);
    return -1;
  }

  close(fd);
  return ifr.ifr_qlen;
}

// set transmit queue length (txqueuelen) for interface
int set_txqueuelen(char* ifname, int num_packets) {

  struct ifreq ifr; 
  int fd;
  int ret;
  
  if((fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
    return -1;
  }

  ifr.ifr_addr.sa_family = AF_INET;
  strncpy(ifr.ifr_name, ifname, IFNAMSIZ);

  ifr.ifr_qlen = num_packets;

  ioctl(fd, SIOCSIFTXQLEN, (caddr_t) &ifr);
  if(ret < 0) {
    perror("Error during SIOCSIFTXQLEN ioctl (set txqueuelen)");
    close(fd);
    return -1;
  }

  close(fd);
  return 0;
}

// get mtu for interface
int get_mtu(char* ifname) {

  struct ifreq ifr; 
  int fd;
  int ret;
  
  if((fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
    return -1;
  }

  ifr.ifr_addr.sa_family = AF_INET;
  strncpy(ifr.ifr_name, ifname, IFNAMSIZ);

  ioctl(fd, SIOCGIFMTU, (caddr_t) &ifr);
  if(ret < 0) {
    perror("Error during SIOCGIFMTU ioctl (get mtu)");
    close(fd);
    return -1;
  }

  close(fd);
  return ifr.ifr_mtu;
}

// set mtu for interface
int set_mtu(char* ifname, int mtu) {

  struct ifreq ifr; 
  int fd;
  int ret;
  
  if((fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
    return -1;
  }

  ifr.ifr_addr.sa_family = AF_INET;
  strncpy(ifr.ifr_name, ifname, IFNAMSIZ);

  ifr.ifr_mtu = mtu; 

  ioctl(fd, SIOCSIFMTU, (caddr_t) &ifr);
  if(ret < 0) {
    perror("Error during SIOCSIFMTU ioctl (set mtu)");
    close(fd);
    return -1;
  }

  close(fd);
  return 0;
}


// baud is specified using macros
// e.g. 9600 is B9600
int open_serial(char* dev, speed_t baud) {

  int fd;

  fd = open(dev, O_RDWR); /* connect to port */
  if(fd < 0) {
    fprintf(stderr, "Failed to open serial device %s: %s\n", dev, strerror(errno));
    return fd;
  }
  
  struct termios settings;
  if(tcgetattr(fd, &settings) < 0) {
    fprintf(stderr, "Failed to get serial device %s attributes: %s\n", dev, strerror(errno));
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
    fprintf(stderr, "Failed to set serial device %s attributes: %s\n", dev, strerror(errno));
    return -1;
  }
  
  tcflush(fd, TCOFLUSH);
  
  return fd;
}

int close_serial(int fd) {
  close(fd);
}


int receive_done(int fds, char* recvd, size_t size) {

  // TODO
  // 
  // 1. If recvd not NULL (something was received)
  //    Send it to TUN interface (fdi)
  // 2. Have another select call in here
  //    to check if there is anything to transmit
  //    Then transmit it. 
  //    Then run rn2903_rx again once transmission is done.
}

int event_loop(int fds, int fdi) {
  int ret;
  int maxfd;
  fd_set fdset;

  ret = rn2903_rx(fds, fdi, RECEIVE_TIME, receive_done);
  if(ret < 0) {
    return ret;
  }

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
      ret = rn2903_read(fds, fdi);
      if(ret < 0) {
        return ret;
      }
    }

    // handle incoming data on network interface
    if(FD_ISSET(fdi, &fdset)) {

      // TODO receive data from fdi
      // TODO parse IP header and ensure that we send only whole packets

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
  char iface_name[IFNAMSIZ] = "lora0";

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

  // Set transmit queue length for TUN interface
  ret = set_txqueuelen(iface_name, TX_QUEUE_LENGTH);
  if(ret < 0) {
    fprintf(stderr, "Unable to set txqueuelen (transmit queue length) for %s interface to %d\n", iface_name, TX_QUEUE_LENGTH);
    return 1;
  }

  // Set MTU for TUN interface
  ret = set_mtu(iface_name, LORA_MTU);
  if(ret < 0) {
    fprintf(stderr, "Unable to set MTU for %s interface to %d\n", iface_name, LORA_MTU);
    return 1;
  }

  // drop root privileges
  ret = drop_privs(RUNAS_GROUP, RUNAS_USER);
  if(ret < 0) {
    fprintf(stderr, "Failed to drop root privileges.\n");
    return 1;
  }

  // socket for talking to the running daemon
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
