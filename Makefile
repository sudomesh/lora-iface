

all: lora_iface

lora_iface: main.c
	$(CC) -o lora_iface main.c

clean:
	rm lora_iface
