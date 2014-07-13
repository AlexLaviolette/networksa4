#include <iostream>
#include <map>
#include <queue>
#include <string.h>
#include <limits.h>
#include <algorithm>
#include "rcsmap.h"
#include "mybind.cpp"
#include "ucp.cpp"


#define CONNECT_TIMEOUT 1000
#define HEADER_LEN 8
#define FLAGS 0
#define SEQ_NUM 2
#define LENGTH 4
#define CHECKSUM 6
#define SYN_BIT 0x4
#define ACK_BIT 0x2
#define FIN_BIT 0x1
#define MSS USHRT_MAX
#define MAX_DATA_SIZE (MSS - HEADER_LEN)

RcsConn::RcsConn(): ucp_sock(ucpSocket()), seq_num(0) {}

RcsConn::RcsConn(const RcsConn & conn):
    ucp_sock(conn.ucp_sock),
    destination(conn.destination),
    queue(conn.queue),
    seq_num(conn.seq_num) {}

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

    unsigned char request[HEADER_LEN] = {0};
    unsigned char response[HEADER_LEN] = {0};

    while (1) {
        errno = 0;
        int length = ucpRecvFrom(ucp_sock, request, HEADER_LEN, addr);
        if (errno & (EAGAIN | EWOULDBLOCK)) continue;
        if (get_length(request) != length - HEADER_LEN) continue;
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

    unsigned int conn_sock = newConn.second.getSocketID();
    ucpSetSockRecvTimeout(conn_sock, CONNECT_TIMEOUT);

    while (1) {
        ucpSendTo(conn_sock, request, HEADER_LEN, addr);
        errno = 0;
        int length = ucpRecvFrom(conn_sock, response, HEADER_LEN, addr);
        if (errno & (EAGAIN | EWOULDBLOCK)) continue;
        if (get_length(request) != length - HEADER_LEN) continue;
        if (is_corrupt(response)) continue;
        if (get_seq_num(request) != 0) continue;
        if ((get_flags(response) & (SYN_BIT|ACK_BIT)) != ACK_BIT) continue;
        break;
    }

    seq_num++;
    return newConn.first;
}

int RcsConn::connect(const sockaddr_in * addr) {
    destination = *addr;

    sockaddr_in local_addr;
    getSockName(&local_addr);

    ucpSetSockRecvTimeout(ucp_sock, CONNECT_TIMEOUT);

    unsigned char request[HEADER_LEN] = {0};
    unsigned char response[HEADER_LEN] = {0};
    set_flags(request, SYN_BIT);
    set_checksum(request);

    while (1) {
        ucpSendTo(ucp_sock, request, HEADER_LEN, &destination);
        errno = 0;
        int length = ucpRecvFrom(ucp_sock, response, HEADER_LEN, &destination);
        if (errno & (EAGAIN | EWOULDBLOCK)) continue;
        if (get_length(request) != length - HEADER_LEN) continue;
        if (is_corrupt(response)) continue;
        if (get_seq_num(request) != 0) continue;
        if ((get_flags(response) & (SYN_BIT|ACK_BIT)) != (SYN_BIT|ACK_BIT)) continue;
        break;
    }

    set_flags(response, ACK_BIT);
    set_checksum(response);
    ucpSendTo(ucp_sock, response, HEADER_LEN, &destination);

    seq_num++;
    return 0;
}

int RcsConn::recv(void * buf, int maxBytes) {
    ucpSetSockRecvTimeout(ucp_sock, CONNECT_TIMEOUT);

    sockaddr_in addr;
    int amount_recvd = 0;

    unsigned char* request = new unsigned char[maxBytes + HEADER_LEN];
    if (!request) return -1;

    while(1) {
        errno = 0;
        amount_recvd = ucpRecvFrom(ucp_sock, request, maxBytes + HEADER_LEN, &addr);
        if (errno & (EAGAIN | EWOULDBLOCK)) continue;
        if (get_length(request) != amount_recvd - HEADER_LEN || is_corrupt(request) || get_seq_num(request) != seq_num) {
            unsigned char response[HEADER_LEN] = {0};
            set_flags(response, ACK_BIT);
            set_seq_num(response, seq_num);
            set_checksum(response);
            ucpSendTo(ucp_sock, response, HEADER_LEN, &addr);
            continue;
        }
        break;
    }

    memcpy(buf, request + HEADER_LEN, amount_recvd - HEADER_LEN);

    unsigned char response[HEADER_LEN] = {0};
    set_flags(response, ACK_BIT);
    set_seq_num(response, seq_num);
    set_checksum(response);
    ucpSendTo(ucp_sock, response, HEADER_LEN, &addr);

    seq_num++;
    return amount_recvd;


}

int RcsConn::send(const void * buf, int numBytes) {
    ucpSetSockRecvTimeout(ucp_sock, CONNECT_TIMEOUT);

    const unsigned char * charBuf = (const unsigned char *) buf;
    unsigned int packet_seq_num = 1;
    for (const unsigned char * chunk = charBuf; chunk - charBuf < numBytes; chunk += MAX_DATA_SIZE) {
        unsigned int dataSize = std::min(MAX_DATA_SIZE, (int) (chunk - charBuf));
        unsigned char * packet = new unsigned char[dataSize + HEADER_LEN];
        if (!packet) {
            while (!queue.empty()) {
                delete queue.front();
                queue.pop();
            }
            return -1;
        }

        memcpy(packet + HEADER_LEN, chunk, dataSize);
        memset(packet, 0, HEADER_LEN);

        set_seq_num(packet, packet_seq_num);
        set_length(packet, dataSize);
        set_checksum(packet);

        packet_seq_num++;
        queue.push(packet);
    }

    while (!queue.empty()) {

        ucpSendTo(ucp_sock, queue.front(), get_length(queue.front()) + HEADER_LEN, &destination);
        unsigned char response[HEADER_LEN] = {0};

        errno = 0;
        int length = ucpRecvFrom(ucp_sock, response, HEADER_LEN, &destination);
        if (errno & (EAGAIN | EWOULDBLOCK)) continue;
        if (get_length(response) != length - HEADER_LEN) continue;
        if (is_corrupt(response)) continue;
        if (get_seq_num(response) != get_seq_num(queue.front())) continue;
        delete queue.front();
        queue.pop();
    }

    return numBytes;
}

int RcsConn::close() {
    return ucpClose(ucp_sock);
}


bool RcsConn::is_corrupt(const unsigned char * packet) {
    unsigned short checksum;
    memcpy(&checksum, packet + CHECKSUM, 2);
    return checksum != calculate_checksum(packet);
}

void RcsConn::set_checksum(unsigned char * packet) {
    unsigned short checksum = calculate_checksum(packet);
    memcpy(packet + CHECKSUM, &checksum, 2);
}

unsigned short RcsConn::calculate_checksum(const unsigned char * packet) {
    unsigned short length = get_length(packet) + HEADER_LEN;
    unsigned short checksum = 0;
    for (const unsigned char * it = packet; it - packet < length; it += 2) {
        if (it - packet == CHECKSUM) continue;
        unsigned short word;
        memcpy(&word, it, 2);
        checksum ^= word;
    }
    return checksum;
}

unsigned short RcsConn::get_flags(const unsigned char * packet) {
    unsigned short flags;
    memcpy(&flags, packet + FLAGS, 2);
    return flags;
}

void RcsConn::set_flags(unsigned char * packet, unsigned short flags) {
    memcpy(packet + FLAGS, &flags, 2);
}

unsigned short RcsConn::get_length(const unsigned char * packet) {
    unsigned short length;
    memcpy(&length, packet + LENGTH, 2);
    return length;
}

void RcsConn::set_length(unsigned char * packet, unsigned short length) {
    memcpy(packet + LENGTH, &length, 2);
}

unsigned short RcsConn::get_seq_num(const unsigned char * packet) {
    unsigned short seq_num;
    memcpy(&seq_num, packet + SEQ_NUM, 2);
    return seq_num;
}

void RcsConn::set_seq_num(unsigned char * packet, unsigned short seq_num) {
    memcpy(packet + SEQ_NUM, &seq_num, 2);
}



RcsMap::RcsMap(): nextId(0) {}

RcsMap::~RcsMap() {}

RcsConn & RcsMap::get(unsigned int sockId) {
    std::map<unsigned int, RcsConn>::iterator it = map.find(sockId);
    if (it == map.end()) {
        throw NotFound();
    }
    return it->second;
}

std::pair<unsigned int, RcsConn &> RcsMap::newConn() {
    std::pair<unsigned int, RcsConn &> ret(nextId, map[nextId]);
    nextId++;
    return ret;
}

int RcsMap::close(unsigned int sockId) {
    std::map<unsigned int, RcsConn>::iterator it = map.find(sockId);
    if (it == map.end()) {
        return -1;
    }
    int ret = it->second.close();
    map.erase(it);
    return ret;
}
