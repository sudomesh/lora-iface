#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#include "rn2903.h"

/*
typedef enum {
  SYS_RESET,
  SYS_VER
} cmd_type;

cmd_type last_cmd;
*/

int (*recv_cb)(char*, size_t) = NULL;

int rn2903_cmd(int fds, char* buf, size_t len, int (*cb)(char*, size_t)) {
  ssize_t sent;
  ssize_t ret;
  const char crlf[] = "\r\n";

  char* to_send = malloc(len + 2);
  if(!to_send) {
    return -1;
  }
  memcpy(to_send, buf, len);
  memcpy(to_send + len, crlf, 2);

  while(sent < len) {
    ret = send(fds, to_send + sent, len - sent, 0);
    if(ret < 0) {
      free(to_send);
      return ret;
    }
    sent += ret;
  }

  free(to_send);
  return 0;
}

int rn2903_sys_ver(int fds) {
  

  return 0;
}

int rn2903_receive(int fds, int fdi, char* data) {

  // TODO implement

  return 0;
}

int rn2903_transmit(int fds, int fdi, char* data) {

  // TODO implement

  return 0;
}
