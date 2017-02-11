
WORK IN PROGRESS. COME BACK LATER.

[![Build Status](https://travis-ci.org/sudomesh/lora-iface.png)](https://travis-ci.org/sudomesh/lora-iface)

lora_iface creates a network interface for a LoRa chip connected over serial, allowing it to be used in an IP network and let's the user manage settings such as channel, spreading factor and transmit power.

Currently only the Microchip RN2903 is supported.


# Transmit queue

lora_iface does not implement a transmit queue. 

While network interface drivers usually implement a simple FIFO transmit queue, the purpose of this queue is to ensure very low transmission delays whenever the network device is ready to send, rather than having to call some kernel API function to request more data which might cause the network device to miss its transmission window.

lora is a very slow protocol compared to both the resource of even the lowliest modern computer, compared to other common network interfaces (wifi, ethernet), and this daemon is implemented in userland. A userland (slow) transmission queue would probably not do anything to speed up transmission.

Instead lora_iface relies on the transmission queue built into the Linux network stack. The size of this transmission queue is shown by ifconfig as `txqueuelen`:

```
ifconfig lora0
```

or as `qlen` by the ip command:

```
ip link show lora0
```

This value is the length of the transmission buffer specified in number of frames (layer 2) though for a TUN interface such as the one used by lora_iface there is no layer 2 so it is the number of IP packets.

The default value is 500 which might actually be a bit much for most lora applications.

txqueuelen can be changed using e.g:

```
ip link set dev lora0 txqueuelen 1000
```

Several queuing disciplines completely ignore txqueuelen, but pfifo_fast, the default linux queueing discipline, uses txqueuelen as its default transmission queue length. You can check which queueing discipline is in use for an interface using e.g:

```
tc qdisc show dev lora0
```

# ToDo

It might be nice to have an option for running in layer 4 mode where the IP header is stripped to save space and all received data then has a fake IP header added which makes it seem like the data was a broadcast packet.

# Copyright and license

Copyright 2016 Marc Juul and Jorrit Poelen

License: GPLv3
