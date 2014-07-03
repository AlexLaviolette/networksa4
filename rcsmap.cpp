#include <map>
#include <queue>
#include "rcsmap.h"


RcsConn::RcsConn(): ucp_sock(ucpSocket()) {}

RcsConn::~RcsConn() {

}

int RcsConn::getSocketID() {
    return ucp_sock;
}

int RcsConn::bind(const sockaddr_in * addr) {

}

int RcsConn::getSockName(sockaddr_in * addr) {

}

int RcsConn::listen() {

}

int RcsConn::accept() {

}

int RcsConn::connect(const sockaddr_in * addr) {

}

int RcsConn::recv(void * buf, int maxBytes) {

}

int RcsConn::send(const void * buf, int numBytes) {

}


RcsMap::RcsMap() {

}

RcsMap::~RcsMap() {

}

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
    if (map.erase(sockId)) {
        return 0;
    } else {
        return -1;
    }
}
