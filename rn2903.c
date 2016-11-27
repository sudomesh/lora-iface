#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#include "rn2903.h"

#define RECEIVE_BUFFER_SIZE 8192

extern int debug;

char rbuf[RECEIVE_BUFFER_SIZE];
size_t rbuf_len = 0;

int (*recv_cb)(char*, size_t) = NULL;

int recv_cb_default(char* buf, size_t len) {
  fprintf(stdout, "Got unexpected data: %s\n", buf);
  return 0;
}

int rn2903_cmd(int fds, char* buf, size_t len, int (*cb)(char*, size_t)) {
  ssize_t sent = 0;
  ssize_t ret;
  const char crlf[] = "\r\n\0";

  // TODO queue command if a command is already processing
  if(recv_cb) {
    return -1;
  }
  recv_cb = cb;

  char* to_send = malloc(len + 3);
  if(!to_send) {
    return -1;
  }
  memcpy(to_send, buf, len);
  memcpy(to_send + len, crlf, 3);
  len += 2;

  if(debug) {
    printf("Sending: %s", to_send);
  }

  while(sent < len) {
    ret = write(fds, to_send + sent, len - sent);
    if(ret < 0) {
      fprintf(stderr, "Error during send to serial: %s\n", strerror(errno));
      free(to_send);
      return ret;
    }
    sent += ret;
  }

  free(to_send);
  return 0;
}

int rn2903_sys_get_ver(int fds, int (*cb)(char*, size_t)) {
  char cmd[] = "sys get ver";

  return rn2903_cmd(fds, cmd, sizeof(cmd)-1, cb);
}


void (*rn2903_check_result_cb)(int) = NULL;

int rn2903_check_result(char* res, size_t len) {
  int ret;
  int i;
  const char expected[] = "RN2903";

  if(!rn2903_check_result_cb) {
    return 0;
  }

  ret = strncmp(res, expected, sizeof(expected)-1);
  if(ret != 0) {
    ret = -1;
  }

  rn2903_check_result_cb(ret);  
  return 0;
}

// Check if an RN2903 chip is connected
// by sending "sys get var"
// and expecting the response to begin with "RN2903".
// Calls the callback with -1 if unexpected return value
// or with 0 if success
int rn2903_check(int fds, void (*cb)(int)) {
  rn2903_check_result_cb = cb;
  return rn2903_sys_get_ver(fds, rn2903_check_result);
}



// check for CRLF
ssize_t rn2903_handle_received(char* buf, size_t len) {
  int i;
  int found = 0;

  for(i=0; i < len - 1; i++) {
    if(buf[i] == '\r' && buf[i+1] == '\n') {
      found = i + 2;
    }
  }
  if(!found) {
    return 0;
  }

  // call callback if set
  if(recv_cb) {
    recv_cb(rbuf, found);
    recv_cb = NULL;
  } else { // or call default handler
    recv_cb_default(rbuf, found);
  }

  return found;
}

ssize_t rn2903_receive(int fds, int fdi) {

  ssize_t ret;
  ssize_t parsed;

  while(1) {
    ret = read(fds, rbuf + rbuf_len, RECEIVE_BUFFER_SIZE - rbuf_len);
    if(ret < 0) {
      if(errno == EAGAIN) {
        return 0;
      }
      return ret;
    }

    if(ret == 0) {
      return 0;
    }

    rbuf_len += ret;

    parsed = rn2903_handle_received(rbuf, rbuf_len);
    if(parsed > 0) {

      // move remaining buffer data to beginning of buffer
      memmove(rbuf, rbuf + parsed, rbuf_len - parsed);
      rbuf_len -= parsed;
    }

    if(rbuf_len >= RECEIVE_BUFFER_SIZE) {
      // TODO what do we do when we run out of buffer?
      return -1;
    }
  }


  return 0;
}

int rn2903_transmit(int fds, int fdi, char* data) {

  // TODO implement

  return 0;
}
