

int rn2903_check();

int rn2903_cmd(int fds, char* buf, size_t len, int (*cb)(char*, size_t));

int rn2903_receive(int fds, int fdi, char* data);

int rn2903_transmit(int fds, int fdi, char* data);
