#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>

#include "rn2903.h"

#define RECEIVE_BUFFER_SIZE 8192

#define CMD_RESP_OK "ok"
#define CMD_RESP_INVALID_PARAM "invalid_param"
#define CMD_RESP_BUSY "busy"

typedef struct command {
  char* buf;
  size_t len;
  int (*cb)(int, char*, size_t);
  struct timespec last_attempt;
} command;

extern int debug;

command* cmd = NULL; // cmd that has been sent but no response received yet

char rbuf[RECEIVE_BUFFER_SIZE];
size_t rbuf_len = 0;

int (*recv_cb)(int fds, char*, size_t) = NULL;

int recv_cb_default(int fds, char* buf, size_t len) {
  fprintf(stdout, "Got unexpected data: %s\n", buf);
  return 0;
}

// send queued command if any
ssize_t rn2903_transmit(int fds) {
  command* cmd;
  char* to_send;
  size_t to_send_len;
  ssize_t sent = 0;
  int ret;
  const char crlf[] = "\r\n\0";

  if(!cmd) {
    return 0; // nothing to do
  }

  to_send = malloc(cmd->len + 3);
  if(!to_send) {
    return -1;
  }
  memcpy(to_send, cmd->buf, cmd->len);
  memcpy(to_send + cmd->len, crlf, 3);
  to_send_len = cmd->len + 2;

  if(debug) {
    printf("Sending: %s", to_send);
  }

  while(sent < to_send_len) {
    ret = write(fds, to_send + sent, cmd->len - sent);
    if(ret < 0) {
      fprintf(stderr, "Error during send to serial: %s\n", strerror(errno));
      free(to_send);
      return ret;
    }
    sent += ret;
  }
  
  free(to_send);
  return sent;
}



int rn2903_cmd(int fds, char* buf, size_t len, int (*cb)(int, char*, size_t)) {
  ssize_t sent = 0;
  const char crlf[] = "\r\n\0";
  char* cmd_str;
  ssize_t ret;

  cmd = malloc(sizeof(command));
  cmd_str = buf;
  cmd->cb = cb;

  ret = rn2903_transmit(fds);

  if(ret < 0) {
    return -1;
  }
  return 0;
}

int rn2903_sys_get_ver(int fds, int (*cb)(int, char*, size_t)) {
  char cmd[] = "sys get ver";

  return rn2903_cmd(fds, cmd, sizeof(cmd)-1, cb);
}


// run the command callback (if any)
// and free the command
int finalize_cmd(int fds, char* buf, size_t size) {
  int ret = 0;
  if(!cmd) return -1;

  if(cmd->cb) {
    ret = cmd->cb(fds, buf, size);
  }

  free(cmd);
  cmd = NULL;

  return ret;
}

// check if two strings are equal, 
// disregarding the last character
// and using the length of the second string
int equals(char* a, const char* b) {
  int ret;

  ret = strncmp(a, b, sizeof(b)-1);
  if(ret == 0) {
    return 1;
  }
  return 0;
}

int rn2903_rx_result2(int fds, char* buf, size_t size) {

  if(equals(buf, "radio_err")) { // reception timeout
    return finalize_cmd(fds, NULL, 0);
  } else if (equals(buf, "radio_rx ")) {
    return finalize_cmd(fds, buf + 9, size - 9);
  } else {
    fprintf(stderr, "Invalid response from rn2903\n");
    return -1;
  }
}

int rn2903_rx_result(int fds, char* buf, size_t size) {

  if(equals(buf, "ok")) {
    recv_cb = rn2903_rx_result2;
  } else if (equals(buf, "invalid_param")) {
    fprintf(stderr, "rn2903 said: 'invalid_param'\n");
    fprintf(stderr, "  in response to command: %s\n", cmd->buf);
    return -1;
  } else if (equals(buf, "busy")) {
    // TODO add a timeout before trying again
    fprintf(stderr, "rn2903 is busy... retrying\n");
    rn2903_transmit(fds);
  } else {
    fprintf(stderr, "Invalid response from rn2903\n");
    return -1;
  }
}

// send the "radio rx" command
int rn2903_rx(int fds, unsigned int rx_window_size, int (*cb)(int, char*, size_t)) {
  char cmd[16];
  if(rx_window_size > 65535) {
    fprintf(stderr, "rx_windows_size must be between 0 and 65535\n");
    return -1;
  }
  snprintf(cmd, 15, "radio rx %ud\n", rx_window_size);
  cmd[15] = '\0'; // just in case

  recv_cb = rn2903_rx_result;

  return rn2903_cmd(fds, cmd, strlen(cmd), cb);
}



int rn2903_check_result(int fds, char* res, size_t len) {
  int ret;
  int i;
  const char expected[] = "RN2903";

  ret = strncmp(res, expected, sizeof(expected)-1);
  if(ret != 0) {
    fprintf(stderr, "Unexpected result from cmd \"sys get var\"\n");
    return finalize_cmd(fds, NULL, 0);
  }

  return finalize_cmd(fds, res, len);
}

// Check if an RN2903 chip is connected
// by sending "sys get var"
// and expecting the response to begin with "RN2903".
// Calls the callback with -1 if unexpected return value
// or with 0 if success
int rn2903_check(int fds, int (*cb)(int, char*, size_t)) {
  recv_cb = rn2903_check_result;
  return rn2903_sys_get_ver(fds, cb);
}



// check for CRLF
ssize_t rn2903_handle_received(int fds, char* buf, size_t len) {
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
    recv_cb(fds, rbuf, found);
    recv_cb = NULL;
  } else { // or call default handler
    recv_cb_default(fds, rbuf, found);
  }

  return found;
}

// read received data from rn2903 via serial
ssize_t rn2903_read(int fds, int fdi) {

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

    parsed = rn2903_handle_received(fds, rbuf, rbuf_len);
    if(parsed > 0) {

      // move remaining buffer data to beginning of buffer
      memmove(rbuf, rbuf + parsed, rbuf_len - parsed);
      rbuf_len -= parsed;
    }

    if(rbuf_len >= RECEIVE_BUFFER_SIZE) {
      fprintf(stderr, "FATAL: Ran out of buffer for rn2903 receive\n");
      // TODO what do we do when we run out of buffer?
      return -1;
    }
  }


  return 0;
}

