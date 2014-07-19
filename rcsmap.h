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
    // Set of all addresses currently bound to this machine
    // (helps in detecting duplicate connection requests)
    typedef std::set<sockaddr_in, sockaddr_comp> addrSet;

    // Maps RCS socket descriptors to corresponding connection objects
    std::map<unsigned int, RcsConn> map;
    addrSet boundAddrs;
    
    // ID of the next RCS socket descriptor to assign
    unsigned int nextId;

    // Mutexes for map and addrSet
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
