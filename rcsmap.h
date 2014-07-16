#ifndef RCSMAP_H
#define RCSMAP_H

#include <map>
#include <deque>
#include <utility>
#include <netinet/in.h>
#include <pthread.h>

class RcsMap;

class RcsConn {
    unsigned int ucp_sock;
    sockaddr_in destination;
    std::deque<unsigned char *> queue;
    unsigned int seq_num;

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

    private:
        bool is_corrupt(const unsigned char * packet);
        unsigned short get_flags(const unsigned char * packet);
        unsigned short get_length(const unsigned char * packet);
        unsigned short get_seq_num(const unsigned char * packet);
        unsigned short calculate_checksum(const unsigned char * packet);
        void set_flags(unsigned char * packet, unsigned short flags);
        void set_length(unsigned char * packet, unsigned short length);
        void set_seq_num(unsigned char * packet, unsigned short seq_num);
        void set_checksum(unsigned char * packet);
        void handleClose();
};

class RcsMap {
    std::map<unsigned int, RcsConn> map;
    unsigned int nextId;
    pthread_mutex_t map_m;

    public:
        class NotFound: public std::exception {};
        RcsMap();
        ~RcsMap();
        RcsConn & get(unsigned int sockId);
        std::pair<unsigned int, RcsConn &> newConn();
        int close(unsigned int sockId);
};

#endif
