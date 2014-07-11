#include <map>
#include <queue>
#include <string.h>
#include "rcsmap.h"
#include "mybind.cpp"

#define CONNECT_TIMEOUT 1000
#define HEADER_LEN 7
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

int RcsConn::accept(sockaddr_in * addr, RcsMap & map) {
    ucpSetSockRecvTimeout(ucp_sock, CONNECT_TIMEOUT);

    char request[HEADER_LEN] = {0};
    char response[HEADER_LEN] = {0};

    while (1) {
        ucpRecvFrom(ucp_sock, request, HEADER_LEN, addr);
        if (errno & (EAGAIN | EWOULDBLOCK)) continue;
        if (is_corrupt(request)) continue;
        if (get_seq_num(request) != 0) continue;
        if ((get_flags(request) & (SYN_BIT|ACK_BIT)) != SYN_BIT) continue;
        break;
    }

    std::pair<unsigned int, RcsConn &> newConn = map.newConn();
    newConn.second.destination = *addr;

    sockaddr_in local_addr;
    getSockName(&local_addr);
    local_addr.sin_port = 0;

    if (!newConn.second.bind(&local_addr)) {
        map.close(newConn.first);
        return -1;
    }

    set_flags(request, SYN_BIT | ACK_BIT);
    set_checksum(request);
    
    unsigned int conn_sock = newConn.second.getSocketId();
    while (1) {
        ucpSendTo(conn_sock, request, HEADER_LEN, addr);
        ucpRecvFrom(conn_sock, response, HEADER_LEN, addr);
        if (errno & (EAGAIN | EWOULDBLOCK)) continue;
        if (is_corrupt(response)) continue;
        if (get_seq_num(request) != 0) continue;
        if ((get_flags(response) & (SYN_BIT|ACK_BIT)) != ACK_BIT) continue;
        break;
    }

    return newConn.first;
}

int RcsConn::connect(const sockaddr_in * addr) {
    destination = *addr;

    sockaddr_in local_addr;
    getSockName(&local_addr);

    ucpSetSockRecvTimeout(ucp_sock, CONNECT_TIMEOUT);

    char request[HEADER_LEN] = {0};
    char response[HEADER_LEN] = {0};
    set_flags(request, SYN_BIT);
    set_checksum(request);

    while (1) {
        ucpSendTo(ucp_sock, request, HEADER_LEN, &destination);
        ucpRecvFrom(ucp_sock, response, HEADER_LEN, &destination);
        if (errno & (EAGAIN | EWOULDBLOCK)) continue;
        if (is_corrupt(response)) continue;
        if (get_seq_num(request) != 0) continue;
        if ((get_flags(response) & (SYN_BIT|ACK_BIT)) != (SYN_BIT|ACK_BIT)) continue;
        break;
    }

    set_flags(response, ACK_BIT);
    set_checksum(response);
    ucpSendTo(ucp_sock, response, HEADER_LEN, &destination);

    return 0;
}

int RcsConn::recv(void * buf, int maxBytes) {

}

int RcsConn::send(const void * buf, int numBytes) {

}

int RcsConn::close() {
    return ucpClose(ucp_sock);
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

std::pair<unsigned int, RcsConn &> RcsMap::newConn() {
    return std::pair(nextId, map[nextId++]);
}

int RcsMap::close(unsigned int sockId) {
    std::map<unsigned int, RcsConn>::iterator it = map.find(sockId);
    if (it == map.end()) {
        return -1;
    }
    int ret = it.second.close();
    map.erase(it);
    return ret;
}
