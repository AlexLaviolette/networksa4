#include <algorithm>
#include <string.h>
#include "packet.h"

bool is_corrupt(const unsigned char * packet) {
    unsigned short checksum;
    memcpy(&checksum, packet + CHECKSUM, 2);
    return checksum != calculate_checksum(packet);
}

void set_checksum(unsigned char * packet) {
    unsigned short checksum = calculate_checksum(packet);
    memcpy(packet + CHECKSUM, &checksum, 2);
}

unsigned short calculate_checksum(const unsigned char * packet) {
    unsigned short length = get_length(packet) + HEADER_LEN;
    unsigned short checksum = 0;
    for (const unsigned char * it = packet; it - packet < length; it += 2) {
        if (it - packet == CHECKSUM) continue;
        unsigned short word = 0;
        memcpy(&word, it, std::min(2, (int) (length - (it - packet))));
        checksum ^= word;
    }
    return checksum;
}

unsigned short get_flags(const unsigned char * packet) {
    unsigned short flags;
    memcpy(&flags, packet + FLAGS, 2);
    return flags;
}

void set_flags(unsigned char * packet, unsigned short flags) {
    memcpy(packet + FLAGS, &flags, 2);
}

unsigned short get_length(const unsigned char * packet) {
    unsigned short length;
    memcpy(&length, packet + LENGTH, 2);
    return length;
}

void set_length(unsigned char * packet, unsigned short length) {
    memcpy(packet + LENGTH, &length, 2);
}

unsigned short get_seq_num(const unsigned char * packet) {
    unsigned short seq_num;
    memcpy(&seq_num, packet + SEQ_NUM, 2);
    return seq_num;
}

void set_seq_num(unsigned char * packet, unsigned short seq_num) {
    memcpy(packet + SEQ_NUM, &seq_num, 2);
}
