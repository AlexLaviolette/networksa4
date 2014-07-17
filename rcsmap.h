#ifndef RCSMAP_H
#define RCSMAP_H

#include <map>
#include <deque>
#include <set>
#include <utility>
#include <netinet/in.h>
#include <pthread.h>

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

struct sockaddr_comp {
    bool operator()(const sockaddr_in & a, const sockaddr_in & b);
};
typedef std::set<sockaddr_in, sockaddr_comp> addrSet;

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

class RcsMap {
    std::map<unsigned int, RcsConn> map;
    addrSet boundAddrs;
    unsigned int nextId;
    pthread_mutex_t map_m;
    pthread_mutex_t addr_m;

    public:
        class NotFound: public std::exception {};
        RcsMap();
        ~RcsMap();
        RcsConn & get(unsigned int sockId);
        bool isBound(const sockaddr_in & addr);
        int accept(unsigned int sockId, sockaddr_in * addr);
        std::pair<unsigned int, RcsConn &> newConn();
        int close(unsigned int sockId);
};

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
