#include <iostream>
#include <map>
#include <errno.h>
#include <deque>
#include <string.h>
#include <limits.h>
#include <algorithm>
#include "rcsmap.h"
#include "mybind.h"
#include "ucp.h"
#include <stdexcept>

#define CONNECT_TIMEOUT 2000
#define ACCEPT_TIMEOUT 1000
#define SEND_TIMEOUT 2000
#define RECV_TIMEOUT 1000
#define SYN_ACK_TIMEOUT 5000

#define HEADER_LEN 8
#define FLAGS 0
#define SEQ_NUM 2
#define LENGTH 4
#define CHECKSUM 6
#define SYN_BIT 0x4
#define ACK_BIT 0x2
#define FIN_BIT 0x1
#define MSS 20000
#define MAX_DATA_SIZE (MSS - HEADER_LEN)

extern int errno;

RcsConn::RcsConn(): ucp_sock(ucpSocket()), seq_num(0) {}

RcsConn::RcsConn(const RcsConn & conn):
    ucp_sock(conn.ucp_sock),
    destination(conn.destination),
    queue(conn.queue),
    seq_num(conn.seq_num) {}

RcsConn::~RcsConn() {
    while (!queue.empty()) {
        delete[] queue.front();
        queue.pop_front();
    }
}

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

void RcsConn::handleClose() {
    unsigned char response[HEADER_LEN] = {0};
    set_flags(response, ACK_BIT);
    set_seq_num(response, 0);
    set_length(response, 0);
    set_checksum(response);
    ucpSendTo(ucp_sock, response, HEADER_LEN, &destination);
    errno = ECONNRESET;
    std::cerr << "GOT FIN. EXITING." << std::endl;
}

int RcsConn::accept(sockaddr_in * addr, RcsMap & map) {
    ucpSetSockRecvTimeout(ucp_sock, ACCEPT_TIMEOUT);

    unsigned char request[HEADER_LEN] = {0};
    unsigned char response[HEADER_LEN] = {0};

    while (1) {
        errno = 0;
        int length = ucpRecvFrom(ucp_sock, request, HEADER_LEN, addr);
        if (errno & (EAGAIN | EWOULDBLOCK)) continue;
        if (get_length(request) != length - HEADER_LEN) continue;
        if (is_corrupt(request)) continue;
        if (get_flags(response) & FIN_BIT) {
            handleClose();
            return -1;
        }
        if (get_seq_num(request) != 0) continue;
        if ((get_flags(request) & (SYN_BIT|ACK_BIT)) != SYN_BIT) continue;
        break;
    }

    std::pair<unsigned int, RcsConn &> newConn = map.newConn();
    newConn.second.destination = *addr;

    sockaddr_in local_addr;
    getSockName(&local_addr);
    local_addr.sin_port = 0;

    if (newConn.second.bind(&local_addr) < 0) {
        map.close(newConn.first);
        return -1;
    }

    set_flags(request, SYN_BIT | ACK_BIT);
    set_checksum(request);

    unsigned int conn_sock = newConn.second.getSocketID();
    ucpSetSockRecvTimeout(conn_sock, SYN_ACK_TIMEOUT);

    while (1) {
        ucpSendTo(conn_sock, request, HEADER_LEN, addr);

        errno = 0;
        int length = ucpRecvFrom(conn_sock, response, HEADER_LEN, addr);
        if (errno & (EAGAIN | EWOULDBLOCK)) continue;
        if (get_length(response) != length - HEADER_LEN) continue;
        if (is_corrupt(response)) continue;
        if (get_flags(response) & FIN_BIT) {
            handleClose();
            return -1;
        }
        if (get_seq_num(response) != 0) continue;
        if ((get_flags(response) & (SYN_BIT|ACK_BIT)) != ACK_BIT) continue;
        break;
    }

    newConn.second.seq_num++;
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
        if (get_length(response) != length - HEADER_LEN) continue;
        if (is_corrupt(response)) continue;
        if (get_flags(response) & FIN_BIT) {
            handleClose();
            return -1;
        }
        if (get_seq_num(response) != 0) continue;
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
    int size = out_queue.pop(buf, maxBytes);
    if (size > 0) {
        return size;
    }

    ucpSetSockRecvTimeout(ucp_sock, RECV_TIMEOUT);

    sockaddr_in addr;
    unsigned char* request = NULL;

    while(1) {
        if (!request) {
            request = new unsigned char[MSS];
            if (!request) return -1;
        }

        errno = 0;
        int amount_recvd = ucpRecvFrom(ucp_sock, request, MSS, &addr);
        if (errno & (EAGAIN | EWOULDBLOCK)) continue;

        std::cerr << "Got packet" << std::endl;

        bool badPacket = get_length(request) != amount_recvd - HEADER_LEN || is_corrupt(request);
        unsigned short request_seq_num = get_seq_num(request);

        if (!badPacket) {
            if (get_flags(request) & FIN_BIT) {
                handleClose();
                delete[] request;
                return -1;
            }

            std::cerr << "Seq num " << request_seq_num << " expected " <<  seq_num << std::endl;
            out_queue.push(request, seq_num);
            request = NULL;
        }

        seq_num = out_queue.getNextSeqNum(seq_num);

        unsigned char response[HEADER_LEN] = {0};
        set_flags(response, ACK_BIT);
        set_seq_num(response, seq_num);
        set_checksum(response);
        ucpSendTo(ucp_sock, response, HEADER_LEN, &addr);

        if (badPacket || request_seq_num > seq_num) continue;
        break;
    }
    return recv(buf, maxBytes);
}

int RcsConn::send(const void * buf, int numBytes) {
    int N = 4;
    ucpSetSockRecvTimeout(ucp_sock, SEND_TIMEOUT);

    const unsigned char * charBuf = (const unsigned char *) buf;
    unsigned int packet_seq_num = seq_num;
    for (const unsigned char * chunk = charBuf; chunk - charBuf < numBytes; chunk += MAX_DATA_SIZE) {
        unsigned int dataSize = std::min(MAX_DATA_SIZE, (int) (numBytes - (chunk - charBuf)));
        unsigned char * packet = new unsigned char[dataSize + HEADER_LEN];
        if (!packet) {
            return -1;
        }

        memcpy(packet + HEADER_LEN, chunk, dataSize);
        memset(packet, 0, HEADER_LEN);

        set_seq_num(packet, packet_seq_num);
        set_length(packet, dataSize);
        set_checksum(packet);

        packet_seq_num++;
        queue.push_back(packet);
    }

    unsigned int next_seq_num = get_seq_num(queue.front());
    while (!queue.empty()) {

        while (next_seq_num < seq_num + N ) {
            try {
                std::cerr << "Sending packet " << get_seq_num(queue.at(next_seq_num - seq_num)) << " expected " << next_seq_num << std::endl;
                ucpSendTo(ucp_sock, queue.at(next_seq_num - seq_num), get_length(queue.at(next_seq_num - seq_num)) + HEADER_LEN, &destination);
            } catch (const std::out_of_range& oor) {
                break;
            }
            next_seq_num++;
        }



        unsigned char response[HEADER_LEN] = {0};
        while (1) {
            errno = 0;
            int length = ucpRecvFrom(ucp_sock, response, HEADER_LEN, &destination);
            if (errno & (EAGAIN | EWOULDBLOCK)) {
                next_seq_num = get_seq_num(queue.front());
                break;
            }
            std::cerr << "Got packet" << std::endl;
            if (get_length(response) != length - HEADER_LEN) break;
            if (is_corrupt(response)) break;
            std::cerr << "Seq num " << get_seq_num(response) << " expected " << seq_num + 1 << std::endl;
            if (get_flags(response) & SYN_BIT) {
                set_flags(response, ACK_BIT);
                set_seq_num(response, 0);
                set_length(response, 0);
                set_checksum(response);
                ucpSendTo(ucp_sock, response, HEADER_LEN, &destination);
                break;
            }
            if (get_flags(response) & FIN_BIT) {
                handleClose();
                return -1;
            }
            if (get_seq_num(response) <= seq_num) continue;

            seq_num = get_seq_num(response);
            while (!queue.empty() && get_seq_num(queue.front()) < seq_num) {
                delete[] queue.front();
                queue.pop_front();
            }
            break;
        }
    }

    return numBytes;
}

int RcsConn::close() {
    int attempts = 3;

    sockaddr_in local_addr;
    getSockName(&local_addr);

    ucpSetSockRecvTimeout(ucp_sock, CONNECT_TIMEOUT);

    unsigned char request[HEADER_LEN] = {0};
    unsigned char response[HEADER_LEN] = {0};
    set_flags(request, FIN_BIT);
    set_checksum(request);

    for(int i = 0; i < attempts; i++) {
        std::cerr << "Sending FIN. attempt " << i << std::endl;
        ucpSendTo(ucp_sock, request, HEADER_LEN, &destination);

        errno = 0;
        int length = ucpRecvFrom(ucp_sock, response, HEADER_LEN, &destination);
        if (errno & (EAGAIN | EWOULDBLOCK)) continue;
        if (get_length(response) != length - HEADER_LEN) continue;
        if (is_corrupt(response)) continue;
        if (get_seq_num(response) != 0) continue;
        if ((get_flags(response) & ACK_BIT) != ACK_BIT) continue;
        break;
    }

    return ucpClose(ucp_sock);
}




RcsMap::RcsMap(): nextId(1) {
    pthread_mutex_init(&map_m, NULL);
}

RcsMap::~RcsMap() {
    pthread_mutex_destroy(&map_m);
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

int RcsMap::close(unsigned int sockId) {
    int ret = -1;
    pthread_mutex_lock(&map_m);
    std::map<unsigned int, RcsConn>::iterator it = map.find(sockId);
    if (it != map.end()) {
        pthread_mutex_unlock(&map_m);
        ret = it->second.close();
        pthread_mutex_lock(&map_m);
        map.erase(it);
    }
    pthread_mutex_unlock(&map_m);
    return ret;
}



OutQueue::OutQueue(): offset(0) {}
OutQueue::~OutQueue() {
    for (firstqueue::iterator it = queue.begin(); it != queue.end(); it++) {
        delete[] it->second;
    }
    for (secondqueue::iterator it = buf.begin(); it != buf.end(); it++) {
        delete[] *it;
    }
}

void OutQueue::push(unsigned char * src, unsigned short cur_seq_num) {
    unsigned short src_seq_num = get_seq_num(src);
    if (src_seq_num < cur_seq_num) {
        delete[] src;
        return;
    }

    firstqueue::iterator existing = queue.find(src_seq_num);
    if (existing != queue.end()) {
        delete[] existing->second;
        existing->second = src;
    } else {
        queue[src_seq_num] = src;
    }

    firstqueue::iterator it = queue.begin();
    while (it != queue.end() && it->first == cur_seq_num) {
        buf.push_back(it->second);
        firstqueue::iterator temp = it;
        it++;
        queue.erase(temp);
        cur_seq_num++;
    }
}

int OutQueue::pop(void * dest, int maxBytes) {
    if (buf.empty()) return -1;
    if (offset + maxBytes >= get_length(buf.front())) {
        maxBytes = get_length(buf.front()) - offset;
        memcpy(dest, buf.front() + HEADER_LEN + offset, maxBytes);
        delete[] buf.front();
        buf.pop_front();
        offset = 0;
    } else {
        memcpy(dest, buf.front() + HEADER_LEN + offset, maxBytes);
        offset += maxBytes;
    }
    return maxBytes;
}

unsigned short OutQueue::getNextSeqNum(unsigned short seq_num) {
    if (buf.empty()) return seq_num;
    return get_seq_num(buf.back()) + 1;
}




bool is_corrupt(const unsigned char * packet) {
    unsigned short checksum;
    memcpy(&checksum, packet + CHECKSUM, 2);
    return checksum != calculate_checksum(packet);
}

void set_checksum(unsigned char * packet) {
    unsigned short checksum = calculate_checksum(packet);
    memcpy(packet + CHECKSUM, &checksum, 2);
}

unsigned short calculate_checksum(const unsigned char * packet) {
    unsigned short length = get_length(packet) + HEADER_LEN;
    unsigned short checksum = 0;
    for (const unsigned char * it = packet; it - packet < length; it += 2) {
        if (it - packet == CHECKSUM) continue;
        unsigned short word = 0;
        memcpy(&word, it, std::min(2, (int) (length - (it - packet))));
        checksum ^= word;
    }
    return checksum;
}

unsigned short get_flags(const unsigned char * packet) {
    unsigned short flags;
    memcpy(&flags, packet + FLAGS, 2);
    return flags;
}

void set_flags(unsigned char * packet, unsigned short flags) {
    memcpy(packet + FLAGS, &flags, 2);
}

unsigned short get_length(const unsigned char * packet) {
    unsigned short length;
    memcpy(&length, packet + LENGTH, 2);
    return length;
}

void set_length(unsigned char * packet, unsigned short length) {
    memcpy(packet + LENGTH, &length, 2);
}

unsigned short get_seq_num(const unsigned char * packet) {
    unsigned short seq_num;
    memcpy(&seq_num, packet + SEQ_NUM, 2);
    return seq_num;
}

void set_seq_num(unsigned char * packet, unsigned short seq_num) {
    memcpy(packet + SEQ_NUM, &seq_num, 2);
}
