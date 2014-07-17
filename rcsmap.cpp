#include <string.h>
#include "rcsmap.h"

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

std::pair<unsigned int, RcsConn &> RcsMap::newConn() {
    pthread_mutex_lock(&map_m);
    std::pair<unsigned int, RcsConn &> ret(nextId, map[nextId]);
    nextId++;
    pthread_mutex_unlock(&map_m);
    return ret;
}

bool RcsMap::isBound(const sockaddr_in & addr) {
    pthread_mutex_lock(&addr_m);
    bool bound = boundAddrs.find(addr) != boundAddrs.end();
    pthread_mutex_unlock(&addr_m);
    return bound;
}

int RcsMap::accept(unsigned int sockId, sockaddr_in * addr) {
    int connSoc = get(sockId).accept(addr, *this);
    if (connSoc > 0) {
        pthread_mutex_lock(&addr_m);
        boundAddrs.insert(*addr);
        pthread_mutex_unlock(&addr_m);
    }
    return connSoc;
}

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

