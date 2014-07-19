#ifndef OUTQUEUE_H
#define OUTQUEUE_H

#include <map>
#include <deque>

// Special object for holding packets bound for the application layer
class OutQueue {

    // Create one "queue" for holding data that cannot be read yet
    // (i.e. there is some packet with a smaller seq number that hasn't been recv-ed)
    // (implemented as a map to prevent duplicate seq numbers/packets)
    typedef std::map<unsigned short, unsigned char *> FirstQueue;

    // Create another "queue" for holding data that can be read
    typedef std::deque<unsigned char *> SecondQueue;

    FirstQueue first_queue;
    SecondQueue second_queue;

    // How many bytes have we already read from the packet at the queue's head?
    int offset;

    public:
        OutQueue();
        OutQueue(const OutQueue & other);
        ~OutQueue();

        // Read "maxBytes" bytes of data from the second queue
        int pop(void * buf, int maxBytes);

        // Push data into the first queue, then move as much data into the second queue as possible
        // (also increments cur_seq_num to the next expected sequence number)
        void push(unsigned char * packet, unsigned short & cur_seq_num);
};

#endif
