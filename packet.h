#ifndef PACKET_H
#define PACKET_H

#define HEADER_LEN 8
#define FLAGS 0
#define SEQ_NUM 2
#define LENGTH 4
#define CHECKSUM 6

#define SEND_BIT 0x8
#define SYN_BIT 0x4
#define ACK_BIT 0x2
#define FIN_BIT 0x1

#define MSS 20000
#define MAX_DATA_SIZE (MSS - HEADER_LEN)

bool is_corrupt(const unsigned char * packet);
unsigned short get_flags(const unsigned char * packet);
unsigned short get_length(const unsigned char * packet);
unsigned short get_seq_num(const unsigned char * packet);
unsigned short calculate_checksum(const unsigned char * packet);
void set_flags(unsigned char * packet, unsigned short flags);
void set_length(unsigned char * packet, unsigned short length);
void set_seq_num(unsigned char * packet, unsigned short seq_num);
void set_checksum(unsigned char * packet);

#endif
