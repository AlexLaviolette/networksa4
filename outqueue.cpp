#include <string.h>
#include "outqueue.h"
#include "packet.h"

OutQueue::OutQueue(): offset(0) {}

OutQueue::OutQueue(const OutQueue & other): offset(other.offset) {
    for (firstqueue::const_iterator it = other.queue.begin(); it != other.queue.end(); it++) {
        int len = strlen((char *)it->second);
        unsigned char * cpy = new unsigned char[len];
        memcpy(cpy, it->second, len);
        queue[it->first] = cpy;
    }
    for (secondqueue::const_iterator it = other.buf.begin(); it != other.buf.end(); it++) {
        int len = strlen((char *)*it);
        unsigned char * cpy = new unsigned char[len];
        memcpy(cpy, *it, len);
        buf.push_back(cpy);
    }
}

OutQueue::~OutQueue() {
    for (firstqueue::iterator it = queue.begin(); it != queue.end(); it++) {
        delete[] it->second;
    }
    for (secondqueue::iterator it = buf.begin(); it != buf.end(); it++) {
        delete[] *it;
    }
}

void OutQueue::push(unsigned char * src, unsigned short cur_seq_num) {
    unsigned short src_seq_num = get_seq_num(src);
    if (src_seq_num < cur_seq_num) {
        delete[] src;
        return;
    }

    firstqueue::iterator existing = queue.find(src_seq_num);
    if (existing != queue.end()) {
        delete[] existing->second;
        existing->second = src;
    } else {
        queue[src_seq_num] = src;
    }

    firstqueue::iterator it = queue.begin();
    while (it != queue.end() && it->first == cur_seq_num) {
        buf.push_back(it->second);
        firstqueue::iterator temp = it;
        it++;
        queue.erase(temp);
        cur_seq_num++;
    }
}

int OutQueue::pop(void * dest, int maxBytes) {
    while (!buf.empty() && (get_flags(buf.front()) & SEND_BIT)) {
        delete[] buf.front();
        buf.pop_front();
    }
    if (buf.empty()) return -1;

    if (offset + maxBytes >= get_length(buf.front())) {
        maxBytes = get_length(buf.front()) - offset;
        memcpy(dest, buf.front() + HEADER_LEN + offset, maxBytes);
        delete[] buf.front();
        buf.pop_front();
        offset = 0;
    } else {
        memcpy(dest, buf.front() + HEADER_LEN + offset, maxBytes);
        offset += maxBytes;
    }

    return maxBytes;
}

unsigned short OutQueue::getNextSeqNum(unsigned short seq_num) {
    if (buf.empty()) return seq_num;
    return get_seq_num(buf.back()) + 1;
}

