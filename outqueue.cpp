#include <iostream>
#include <string.h>
#include "outqueue.h"
#include "packet.h"

OutQueue::OutQueue(): offset(0) {}

// Copy constructor, create copies of packets
OutQueue::OutQueue(const OutQueue & other): offset(other.offset) {
    for (FirstQueue::const_iterator it = other.first_queue.begin(); it != other.first_queue.end(); it++) {
        int len = strlen((char *)it->second);
        unsigned char * cpy = new unsigned char[len];
        memcpy(cpy, it->second, len);
        first_queue[it->first] = cpy;
    }
    for (SecondQueue::const_iterator it = other.second_queue.begin(); it != other.second_queue.end(); it++) {
        int len = strlen((char *)*it);
        unsigned char * cpy = new unsigned char[len];
        memcpy(cpy, *it, len);
        second_queue.push_back(cpy);
    }
}

// Delete all queued packets
OutQueue::~OutQueue() {
    for (FirstQueue::iterator it = first_queue.begin(); it != first_queue.end(); it++) {
        delete[] it->second;
    }
    for (SecondQueue::iterator it = second_queue.begin(); it != second_queue.end(); it++) {
        delete[] *it;
    }
}

void OutQueue::push(unsigned char * packet, unsigned short & cur_seq_num) {
    // If the packet has a smaller sequence number than current sequence number,
    // delete the packet and don't push (this is where we address duplicate packets)
    unsigned short src_seq_num = get_seq_num(packet);
    if (src_seq_num < cur_seq_num) {
        delete[] packet;
        return;
    }

    // If the packet has already been queued, replace the existing packet
    FirstQueue::iterator existing = first_queue.find(src_seq_num);
    if (existing != first_queue.end()) {
        delete[] existing->second;
        existing->second = packet;
    } else {
        first_queue[src_seq_num] = packet;
    }

    // If we have filled in a sequence number "gap", then move as many packets
    // from the first queue to the second queue as possible
    FirstQueue::iterator it = first_queue.begin();
    while (it != first_queue.end() && it->first == cur_seq_num) {
        second_queue.push_back(it->second);
        FirstQueue::iterator temp = it;
        it++;
        first_queue.erase(temp);
        cur_seq_num++;
    }
}

int OutQueue::pop(void * buf, int maxBytes) {
    // Disregard any queued SEND packets (see RcsConn::send())
    while (!second_queue.empty() && (get_flags(second_queue.front()) & SEND_BIT)) {
        delete[] second_queue.front();
        second_queue.pop_front();
    }
    if (second_queue.empty()) return -1;

    if (offset + maxBytes >= get_length(second_queue.front())) {
        // Read all of the data from the head packet,
        // then pop that packet off and delete it
        maxBytes = get_length(second_queue.front()) - offset;
        memcpy(buf, second_queue.front() + HEADER_LEN + offset, maxBytes);
        delete[] second_queue.front();
        second_queue.pop_front();
        offset = 0;
    } else {
        // Read "maxBytes" bytes of data from the head packet
        memcpy(buf, second_queue.front() + HEADER_LEN + offset, maxBytes);
        offset += maxBytes;
    }

    return maxBytes;
}
