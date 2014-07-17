#ifndef RCSCONN_H
#define RCSCONN_H

#include <deque>
#include <netinet/in.h>
#include "outqueue.h"

class RcsMap;

class RcsConn {
    unsigned int ucp_sock;
    sockaddr_in destination;
    std::deque<unsigned char *> queue;
    OutQueue out_queue;
    unsigned int seq_num;
    bool closed;

    public:
        RcsConn();
        RcsConn(const RcsConn & conn);
        ~RcsConn();
        int bind(sockaddr_in * addr);
        int getSockName(sockaddr_in * addr);
        int listen();
        int accept(sockaddr_in * addr, RcsMap & map);
        int connect(const sockaddr_in * addr);
        int recv(void * buf, int maxBytes);
        int send(const void * buf, int numBytes);
        int close();
        int getSocketID();
        const sockaddr_in & getDestination();

    private:
        void handleClose();
};

#endif
