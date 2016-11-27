

all: lora_iface

lora_iface: main.c ipc.c ipc.h rn2903.c rn2903.h 
	$(CC) -o lora_iface main.c ipc.c rn2903.c

clean:
	rm lora_iface	
