

all: lora_iface

lora_iface: main.c ipc.c ipc.h
	$(CC) -o lora_iface main.c ipc.c

clean:
	rm lora_iface	
