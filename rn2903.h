

int rn2903_check();

int rn2903_cmd(int fds, char* buf, size_t len, int (*cb)(char*, size_t));

ssize_t rn2903_receive(int fds, int fdi);

int rn2903_transmit(int fds, int fdi, char* data);
