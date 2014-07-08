#include <map>
#include <queue>
#include "rcsmap.h"
#include "mybind.cpp"


RcsConn::RcsConn(): ucp_sock(ucpSocket()) {}

RcsConn::~RcsConn() {}

int RcsConn::getSocketID() {
    return ucp_sock;
}

int RcsConn::bind(sockaddr_in * addr) {
    return mybind(ucp_sock, addr);
}

int RcsConn::getSockName(sockaddr_in * addr) {
    return ucpGetSockName(ucp_sock, addr);
}

int RcsConn::listen() {
    return 0;
}

int RcsConn::accept(sockaddr_in * addr) {
    
}

int RcsConn::connect(const sockaddr_in * addr) {

}

int RcsConn::recv(void * buf, int maxBytes) {

}

int RcsConn::send(const void * buf, int numBytes) {

}


RcsMap::RcsMap(): nextId(0) {}

RcsMap::~RcsMap() {}

RcsConn & RcsMap::get(unsigned int sockId) {
    std::map<unsigned int, RcsConn>::iterator it = map.find(sockId);
    if (it == map.end()) {
        throw NotFound();
    }
    return it.second;
}

RcsConn & RcsMap::newConn() {
    return map[nextId++];
}

int RcsMap::close(unsigned int sockId) {
    std::map<unsigned int, RcsConn>::iterator it = map.find(sockId);
    if (it == map.end()) {
        return -1;
    }
    int ret = ucpClose(it.second.getSockId());
    map.erase(it);
    return ret;
}
