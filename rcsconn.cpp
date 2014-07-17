#include <algorithm>
#include <iostream>
#include <stdexcept>
#include <utility>
#include <errno.h>
#include <string.h>
#include "packet.h"
#include "rcsconn.h"
#include "rcsmap.h"
#include "ucp.h"

#define CONNECT_TIMEOUT 50
#define ACCEPT_TIMEOUT 50
#define SEND_TIMEOUT 50
#define RECV_TIMEOUT 50
#define SYN_ACK_TIMEOUT 500

#define MAX_SYN_ACK_RETRIES 10
#define MAX_SYN_RETRIES 100
#define MAX_RECV_RETRIES 1000
#define MAX_SEND_RETRIES 50
#define MAX_CLOSE_RETRIES 10

#define WINDOW_SIZE 4

extern int errno;

// Create new unbound connection with new UCP socket
RcsConn::RcsConn():
        ucp_sock(ucpSocket()), seq_num(0), closed(false) {
    memset(&destination, 0, sizeof(sockaddr_in));
}

// Copy constructor
RcsConn::RcsConn(const RcsConn & conn):
        ucp_sock(conn.ucp_sock),
        destination(conn.destination),
        out_queue(conn.out_queue),
        seq_num(conn.seq_num),
        closed(conn.closed) {
    for (std::deque<unsigned char *>::const_iterator it = conn.queue.begin(); it != conn.queue.end(); it++) {
        int len = strlen((char *)*it);
        unsigned char * cpy = new unsigned char[len];
        memcpy(cpy, *it, len);
        queue.push_back(cpy);
    }
}

RcsConn::~RcsConn() {
    while (!queue.empty()) {
        delete[] queue.front();
        queue.pop_front();
    }
}

int RcsConn::getSocketID() {
    return ucp_sock;
}

const sockaddr_in & RcsConn::getDestination() {
    return destination;
}


int RcsConn::bind(sockaddr_in * addr) {
    return ucpBind(ucp_sock, addr);
}

int RcsConn::getSockName(sockaddr_in * addr) {
    return ucpGetSockName(ucp_sock, addr);
}

int RcsConn::listen() {
    return 0;
}

// Accept incoming connection, create new RcsConn in map, and store destination in "addr"
int RcsConn::accept(sockaddr_in * addr, RcsMap & map) {
    if (closed) return -1;

    ucpSetSockRecvTimeout(ucp_sock, ACCEPT_TIMEOUT);

    unsigned char request[HEADER_LEN] = {0};
    unsigned char response[HEADER_LEN] = {0};

    while (1) {
        errno = 0;
        int length = ucpRecvFrom(ucp_sock, request, HEADER_LEN, addr);
        if (errno & (EAGAIN | EWOULDBLOCK)) continue;
        if (errno & ECONNRESET) {
            close();
            return -1;
        }
        if (length < 0) return length;

        if (get_length(request) != length - HEADER_LEN) continue;
        if (is_corrupt(request)) continue;
        if (get_seq_num(request) != 0) continue;
        if ((get_flags(request) & (SYN_BIT|ACK_BIT)) != SYN_BIT) continue;
        if (map.isBound(*addr)) continue;
        break;
    }

    std::pair<unsigned int, RcsConn &> newConn(map.newConn());

    sockaddr_in local_addr;
    getSockName(&local_addr);
    local_addr.sin_port = 0;

    if (newConn.second.bind(&local_addr) < 0) {
        map.close(newConn.first);
        return -1;
    }

    set_flags(request, SYN_BIT | ACK_BIT);
    set_checksum(request);

    unsigned int conn_sock = newConn.second.ucp_sock;
    ucpSetSockRecvTimeout(conn_sock, SYN_ACK_TIMEOUT);
    newConn.second.destination = *addr;

    unsigned int unsuccessful_reads = 0;
    while (1) {
        ucpSendTo(conn_sock, request, HEADER_LEN, addr);

        errno = 0;
        int length = ucpRecvFrom(conn_sock, response, HEADER_LEN, addr);
        if (errno & (EAGAIN | EWOULDBLOCK)) {
            unsuccessful_reads++;
            if (unsuccessful_reads == MAX_SYN_ACK_RETRIES) {
                //std::cerr << "MAX_SYN_ACK_RETRIES" << std::endl;
                newConn.second.close();
                return -1;
            }
            continue;
        }
        unsuccessful_reads = 0;

        if (errno & ECONNRESET) {
            close();
            return -1;
        }
        if (length < 0) return length;

        if (get_length(response) != length - HEADER_LEN) continue;
        if (is_corrupt(response)) continue;
        if (get_seq_num(response) != 0) continue;
        if ((get_flags(response) & (SYN_BIT|ACK_BIT)) != ACK_BIT) continue;
        break;
    }

    newConn.second.seq_num++;
    return newConn.first;
}

int RcsConn::connect(const sockaddr_in * addr) {
    if (closed) return -1;

    destination = *addr;

    sockaddr_in local_addr;
    getSockName(&local_addr);

    ucpSetSockRecvTimeout(ucp_sock, CONNECT_TIMEOUT);

    unsigned char request[HEADER_LEN] = {0};
    unsigned char response[HEADER_LEN] = {0};
    set_flags(request, SYN_BIT);
    set_checksum(request);

    unsigned int unsuccessful_reads = 0;
    while (1) {
        ucpSendTo(ucp_sock, request, HEADER_LEN, &destination);

        errno = 0;
        int length = ucpRecvFrom(ucp_sock, response, HEADER_LEN, &destination);
        if (errno & (EAGAIN | EWOULDBLOCK)) {
            unsuccessful_reads++;
            if (unsuccessful_reads == MAX_SYN_RETRIES) {
                //std::cerr << "MAX_SYN_RETRIES" << std::endl;
                close();
                return -1;
            }
            continue;
        }
        unsuccessful_reads = 0;

        if (errno & ECONNRESET) {
            close();
            return -1;
        }
        if (length < 0) return length;

        if (get_length(response) != length - HEADER_LEN) continue;
        if (is_corrupt(response)) continue;
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
    if (closed) return -1;

    ucpSetSockRecvTimeout(ucp_sock, RECV_TIMEOUT);

    sockaddr_in addr;
    unsigned char* request = NULL;

    unsigned int unsuccessful_reads = 0;
    while(1) {
        if (!request) {
            request = new unsigned char[MSS];
            if (!request) return -1;
        }

        errno = 0;
        int amount_recvd = ucpRecvFrom(ucp_sock, request, MSS, &addr);
        if (errno & (EAGAIN | EWOULDBLOCK)) {
            unsuccessful_reads++;
            if (unsuccessful_reads > MAX_RECV_RETRIES) {
                //std::cerr << "MAX_RECV_RETRIES" << std::endl;
                close();
                delete[] request;
                return -1;
            }
            continue;
        }
        unsuccessful_reads = 0;

        if (errno & ECONNRESET) {
            close();
            delete[] request;
            return -1;
        }
        if (amount_recvd < 0) {
            delete[] request;
            return amount_recvd;
        }

        //std::cerr << "Got packet" << std::endl;

        bool badPacket = get_length(request) != amount_recvd - HEADER_LEN || is_corrupt(request);
        bool isExpectedPacket = false;

        if (!badPacket) {
            unsigned short request_seq_num = get_seq_num(request);
            if (request_seq_num >= seq_num) {
                //std::cerr << "Seq num " << request_seq_num << " expected " <<  seq_num << std::endl;
                out_queue.push(request, seq_num);
                request = NULL;
                isExpectedPacket = request_seq_num == seq_num;
            }
        }

        seq_num = out_queue.getNextSeqNum(seq_num);

        unsigned char response[HEADER_LEN] = {0};
        set_flags(response, ACK_BIT);
        set_seq_num(response, seq_num);
        set_checksum(response);
        ucpSendTo(ucp_sock, response, HEADER_LEN, &addr);

        if (badPacket || !isExpectedPacket) continue;
        break;
    }
    return recv(buf, maxBytes);
}

int RcsConn::send(const void * buf, int numBytes) {
    if (closed) return -1;

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
    unsigned int unsuccessful_reads = 0;

    while (!queue.empty()) {
        while (next_seq_num < seq_num + WINDOW_SIZE) {
            try {
                //std::cerr << "Sending packet " << get_seq_num(queue.at(next_seq_num - seq_num)) << " expected " << next_seq_num << std::endl;
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
                unsuccessful_reads++;
                if (unsuccessful_reads == MAX_SEND_RETRIES) {
                    //std::cerr << "MAX_SEND_RETRIES" << std::endl;
                    close();
                    return -1;
                }
                next_seq_num = get_seq_num(queue.front());
                break;
            }
            unsuccessful_reads = 0;

            if (errno & ECONNRESET) {
                close();
                return -1;
            }
            if (length < 0) return length;

            //std::cerr << "Got packet" << std::endl;
            if (get_length(response) != length - HEADER_LEN) break;
            if (is_corrupt(response)) break;
            //std::cerr << "Seq num " << get_seq_num(response) << " expected " << seq_num + 1 << std::endl;
            if (get_flags(response) & SYN_BIT) {
                set_flags(response, ACK_BIT);
                set_seq_num(response, 0);
                set_length(response, 0);
                set_checksum(response);
                ucpSendTo(ucp_sock, response, HEADER_LEN, &destination);
                break;
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
    if (closed) return 0;

    closed = true;
    return ucpClose(ucp_sock);
}

