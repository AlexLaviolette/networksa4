#include <map>
#include <queue>
#include "rcsmap.h"
#include "mybind.cpp"

#define CONNECT_TIMEOUT 1000
#define HEADER_LEN 5
#define SYN_BIT 0x4
#define ACK_BIT 0x2
#define FIN_BIT 0x1

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
    destination = *addr;

    sockaddr_in local_addr;
    getSockName(&local_addr);

    ucpSetSockRecvTimeout(ucp_sock, CONNECT_TIMEOUT);

    char request[64] = {0};
    char response[64] = {0};
    memcpy(request + HEADER_LEN, &local_addr, sizeof(sockaddr_in));
    set_flags(request, SYN_BIT);
    set_length(request, sizeof(sockaddr_in));
    set_checksum(request);

    while (1) {
        ucpSendTo(ucp_sock, request, HEADER_LEN + sizeof(sockaddr_in), &destination);
        ucpRecvFrom(ucp_sock, response, 64, &destination);
        if (errno & (EAGAIN | EWOULDBLOCK)) continue;
        if (is_corrupt(response)) continue;
        if ((get_flags(response) & (SYN_BIT|ACK_BIT)) != (SYN_BIT|ACK_BIT)) continue;
        break;
    }

    set_flags(response, ACK_BIT);
    set_checksum(response);
    ucpSendTo(ucp_sock, response, HEADER_LEN + get_length(response), &destination);

    return 0;
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
