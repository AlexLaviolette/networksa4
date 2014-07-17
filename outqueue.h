#ifndef OUTQUEUE_H
#define OUTQUEUE_H

#include <map>
#include <deque>

class OutQueue {
    typedef std::map<unsigned short, unsigned char *> firstqueue;
    typedef std::deque<unsigned char *> secondqueue;
    firstqueue queue;
    secondqueue buf;
    int offset;

    public:
        OutQueue();
        OutQueue(const OutQueue & other);
        ~OutQueue();
        int pop(void * dest, int maxBytes);
        void push(unsigned char * buf, unsigned short cur_seq_num);
        unsigned short getNextSeqNum(unsigned short seq_num);
};

#endif
