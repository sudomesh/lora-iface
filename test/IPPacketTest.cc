#include <gtest/gtest.h>
#include <net/ethernet.h>   
#include <netinet/ip_icmp.h>   
#include <netinet/udp.h>   
#include <netinet/tcp.h>   
#include <netinet/ip.h>  

TEST(IPPacketTest, ConfirmLittleEndian) {
  //only passes on little endian machines
  int one = 1;
  char *ptr;
  ptr = (char *)&one;
  ASSERT_EQ('\x1', *ptr);
  ASSERT_EQ(4, sizeof(one)); 
  ASSERT_EQ(16777216, htonl(one)); 
  ASSERT_EQ(1, ntohl(htonl(one))); 
}


TEST(IPPacketTest, ParsePacket) {

  // ping www.google.com request captured using wireshark
  static const unsigned char pingRequest[98] = {
    0xc0, 0xc1, 0xc0, 0xcb, 0x88, 0x2d, 0x00, 0x23, /* .....-.# */
    0x6c, 0x82, 0xc0, 0x1b, 0x08, 0x00, 0x45, 0x00, /* l.....E. */
    0x00, 0x54, 0xe1, 0x18, 0x40, 0x00, 0x40, 0x01, /* .T..@.@. */
    0xe5, 0x5c, 0xc0, 0xa8, 0x01, 0x64, 0xac, 0xd9, /* .\...d.. */
    0x05, 0x4e, 0x08, 0x00, 0x6a, 0x5c, 0x6b, 0x8a, /* .N..j\k. */
    0x00, 0x02, 0x60, 0xda, 0x48, 0x58, 0x00, 0x00, /* ..`.HX.. */
    0x00, 0x00, 0xad, 0x11, 0x0d, 0x00, 0x00, 0x00, /* ........ */
    0x00, 0x00, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, /* ........ */
    0x16, 0x17, 0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, /* ........ */
    0x1e, 0x1f, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25, /* .. !"#$% */
    0x26, 0x27, 0x28, 0x29, 0x2a, 0x2b, 0x2c, 0x2d, /* &'()*+,- */
    0x2e, 0x2f, 0x30, 0x31, 0x32, 0x33, 0x34, 0x35, /* ./012345 */
    0x36, 0x37                                      /* 67 */
  };

  // layer 2 - data link / ethernet
  struct ethhdr *ethernetHeader = (struct ethhdr*)pingRequest;

  ASSERT_EQ(14, sizeof(ethhdr)); 
  // source/ dest are media access control (mac) addresses (layer 1)  
  ASSERT_EQ(0x1b, ethernetHeader->h_source[5]);
  ASSERT_EQ(0x2d, ethernetHeader->h_dest[5]);


  // layer 3 - network / internet protocol
  struct iphdr *ipHeader = (struct iphdr*)(pingRequest + sizeof(struct ethhdr));
  
  int ipHeaderLength = ipHeader->ihl * 4;
  ASSERT_EQ(1, ipHeader->protocol);
  ASSERT_EQ(84, ntohs(ipHeader->tot_len));
  ASSERT_EQ(20, ipHeaderLength); 
 
  struct sockaddr_in source;
  memset(&source, 0, sizeof(struct sockaddr_in));
  source.sin_addr.s_addr = ipHeader->saddr;
  char *ipSource = inet_ntoa(source.sin_addr);
  ASSERT_STREQ("192.168.1.100", ipSource);

  struct sockaddr_in dest;
  memset(&dest, 0, sizeof(struct sockaddr_in));
  dest.sin_addr.s_addr = ipHeader->daddr;
  char *ipDest = inet_ntoa(dest.sin_addr);
  ASSERT_STREQ("172.217.5.78", ipDest);

  // layer 4 - protocol / internet control message protocol
  unsigned short icmpHeaderOffset = sizeof(struct ethhdr) + ipHeaderLength; 
  struct icmphdr *icmpHeader = (struct icmphdr*)(pingRequest + icmpHeaderOffset);

  ASSERT_EQ(8, sizeof(struct icmphdr));
  ASSERT_EQ(ICMP_ECHO, icmpHeader->type);
}
