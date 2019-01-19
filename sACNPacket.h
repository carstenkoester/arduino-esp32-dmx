#ifndef SACN_PACKET_H
#define SACN_PACKET_H
/*
 * sACN packet structure
 *
 * The definition of the sACN packet, as an overlay to a buffer area
 *
 * This is taken from:
 * http://www.cs.utah.edu/~hollaar/Theater/Programs/WiFi-sACN.cpp
 *
 */
static union sACNPacket {
  struct {
    struct {  // ACN Root Layer
      unsigned char preamble_size[2]; // 0x0010
      unsigned char postable_size[2]; // 0x0000
      unsigned char packet_id[12];    // ASC-E1.17
      unsigned char flags_and_len[2]; // 0x7 plus len
      unsigned char vector[4];      // 0x00000004
      unsigned char CID[16];
    } Root;
    struct {  // ACN Framing Layer
      unsigned char flags_and_len[2]; // 0x7 plus len
      unsigned char vector[4];      // 0x00000002
      unsigned char source_name[64];
      unsigned char priority;
      unsigned char reserved[2];    // ignore
      unsigned char seq_num;
      unsigned char options;
      unsigned char universe[2];
    } Frame;
    struct {  // ACN DMP Layer
      unsigned char flags_and_len[2]; // 0x7 plus len
      unsigned char vector;       // 0x02
      unsigned char ad_types;     // 0xA1
      unsigned char prot_addr[2];   // 0x0000
      unsigned char prop_addr_incr[2];  // 0x0001
      unsigned char prop_count[2];    // 1 to 513
      unsigned char slots[513];     // DMX slots
      } DMP;
  } packet;
  unsigned char buffer[sizeof(packet)];
} sACNPacket;
# endif
