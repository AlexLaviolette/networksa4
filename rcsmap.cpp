#include <string.h>
#include "rcsmap.h"

// Required to implement a set of sockaddr_ins
bool RcsMap::sockaddr_comp::operator()(const sockaddr_in & a, const sockaddr_in & b) {
    return memcmp(&a, &b, sizeof(sockaddr_in)) < 0;
}

RcsMap::RcsMap(): nextId(1) {
    pthread_mutex_init(&map_m, NULL);
    pthread_mutex_init(&addr_m, NULL);
}

RcsMap::~RcsMap() {
    pthread_mutex_destroy(&map_m);
    pthread_mutex_destroy(&addr_m);
}

// Get the connection object associated with RCS socket descriptor "sockId"
RcsConn & RcsMap::get(unsigned int sockId) {
    pthread_mutex_lock(&map_m);
    std::map<unsigned int, RcsConn>::iterator it = map.find(sockId);
    std::map<unsigned int, RcsConn>::iterator end = map.end();
    pthread_mutex_unlock(&map_m);
    if (it == end) {
        throw NotFound();
    }
    return it->second;
}

// Create a new connection and return both the RCS socket descriptor
// and the associated connection object
std::pair<unsigned int, RcsConn &> RcsMap::newConn() {
    pthread_mutex_lock(&map_m);
    std::pair<unsigned int, RcsConn &> ret(nextId, map[nextId]);
    nextId++;
    pthread_mutex_unlock(&map_m);
    return ret;
}

// Has this socket address been bound to this machine already?
bool RcsMap::isBound(const sockaddr_in & addr) {
    pthread_mutex_lock(&addr_m);
    bool bound = boundAddrs.find(addr) != boundAddrs.end();
    pthread_mutex_unlock(&addr_m);
    return bound;
}

// Special method for accepting new connections from a passive socket
// (register the new address before returning)
int RcsMap::accept(unsigned int sockId, sockaddr_in * addr) {
    int connSoc = get(sockId).accept(addr, *this);
    if (connSoc > 0) {
        pthread_mutex_lock(&addr_m);
        boundAddrs.insert(*addr);
        pthread_mutex_unlock(&addr_m);
    }
    return connSoc;
}

// Special method for closing connections
// (deregister the connection's destination address before returning)
int RcsMap::close(unsigned int sockId) {
    int ret = -1;
    pthread_mutex_lock(&map_m);
    std::map<unsigned int, RcsConn>::iterator it = map.find(sockId);
    if (it != map.end()) {
        pthread_mutex_unlock(&map_m);

        ret = it->second.close();

        pthread_mutex_lock(&addr_m);
        boundAddrs.erase(it->second.getDestination());
        pthread_mutex_unlock(&addr_m);

        pthread_mutex_lock(&map_m);
        map.erase(it);
    }
    pthread_mutex_unlock(&map_m);
    return ret;
}

