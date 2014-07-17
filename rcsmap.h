#ifndef RCSMAP_H
#define RCSMAP_H

#include <map>
#include <set>
#include <utility>
#include <pthread.h>
#include "rcsconn.h"

class RcsMap {
    struct sockaddr_comp {
        bool operator()(const sockaddr_in & a, const sockaddr_in & b);
    };
    typedef std::set<sockaddr_in, sockaddr_comp> addrSet;
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

#endif
