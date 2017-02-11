

int rn2903_check();

int rn2903_cmd(int fds, char* buf, size_t len, int (*cb)(char*, size_t));

// read received data from rn2903 via serial
ssize_t rn2903_read(int fds, int fdi);

ssize_t rn2903_transmit(int fds);
