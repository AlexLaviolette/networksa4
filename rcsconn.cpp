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
#define CLOSE_TIMEOUT 500

#define MAX_SYN_ACK_RETRIES 10
#define MAX_SYN_RETRIES 100
#define MAX_RECV_RETRIES 1000
#define MAX_SEND_RETRIES 500
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

// Deallocate everything that might still be in the queue
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

// Got FIN packet, send ACK response and exit
void RcsConn::handleClose() {
    if (closed) return;

    if (destination.sin_addr.s_addr) {
        unsigned char response[HEADER_LEN] = {0};
        set_flags(response, ACK_BIT);
        set_seq_num(response, seq_num);
        set_length(response, 0);
        set_checksum(response);
        ucpSendTo(ucp_sock, response, HEADER_LEN, &destination);
    }

    ucpClose(ucp_sock);
    errno = ECONNRESET;
    closed = true;
}

// Accept incoming connection, create new RcsConn in map, and store destination in "addr"
int RcsConn::accept(sockaddr_in * addr, RcsMap & map) {
    if (closed) return -1;

    ucpSetSockRecvTimeout(ucp_sock, ACCEPT_TIMEOUT);

    unsigned char request[HEADER_LEN] = {0};
    unsigned char response[HEADER_LEN] = {0};

    // Get incoming connection request
    while (1) {
        errno = 0;
        int length = ucpRecvFrom(ucp_sock, request, HEADER_LEN, addr);
        if ((errno == EAGAIN) || (errno == EWOULDBLOCK)) continue;
        if (length < 0) return length;

        if (get_length(request) != length - HEADER_LEN) continue;
        if (is_corrupt(request)) continue;
        if (get_seq_num(request) != 0) continue;
        if ((get_flags(request) & (SYN_BIT|ACK_BIT)) != SYN_BIT) continue;

        // Detect and prevent duplicate connection attempts
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

    // Rewrite request as SYN-ACK (with zero sequence number)
    set_flags(request, SYN_BIT | ACK_BIT);
    set_seq_num(request, 0);
    set_checksum(request);

    unsigned int conn_sock = newConn.second.ucp_sock;
    ucpSetSockRecvTimeout(conn_sock, SYN_ACK_TIMEOUT);
    newConn.second.destination = *addr;

    // Perform the rest of the handshake on the new connection's socket
    unsigned int unsuccessful_reads = 0;
    while (1) {
        ucpSendTo(conn_sock, request, HEADER_LEN, addr);

        errno = 0;
        int length = ucpRecvFrom(conn_sock, response, HEADER_LEN, addr);
        if ((errno == EAGAIN) || (errno == EWOULDBLOCK)) {
            unsuccessful_reads++;
            if (unsuccessful_reads == MAX_SYN_ACK_RETRIES) {
                // If the client doesn't respond, we want to close the connection
                // then try another accept
                newConn.second.seq_num++;
                newConn.second.handleClose();
                return accept(addr, map);
            }
            continue;
        }
        unsuccessful_reads = 0;

        if (length < 0) return length;
        if (get_length(response) != length - HEADER_LEN) continue;
        if (is_corrupt(response)) continue;
        if (get_flags(response) & FIN_BIT) {
            // If the connection is closing, then try another accept
            newConn.second.seq_num++;
            newConn.second.handleClose();
            return accept(addr, map);
        }

        // We must get an ACK before the connection can be considered established
        if ((get_flags(response) & (SYN_BIT|ACK_BIT)) != ACK_BIT) continue;
        break;
    }

    newConn.second.seq_num++;
    return newConn.first;
}

int RcsConn::connect(const sockaddr_in * addr) {
    if (closed) return -1;

    // Manipulate address information in this buffer until connected
    sockaddr_in remote_addr = *addr;

    ucpSetSockRecvTimeout(ucp_sock, CONNECT_TIMEOUT);

    // Request is a SYN packet (with zero sequence number)
    // Response will be a SYN-ACK packet (with zero sequence number)
    unsigned char request[HEADER_LEN] = {0};
    unsigned char response[HEADER_LEN] = {0};
    set_flags(request, SYN_BIT);
    set_checksum(request);

    unsigned int unsuccessful_reads = 0;
    while (1) {
        ucpSendTo(ucp_sock, request, HEADER_LEN, &remote_addr);

        errno = 0;
        int length = ucpRecvFrom(ucp_sock, response, HEADER_LEN, &remote_addr);
        if ((errno == EAGAIN) || (errno == EWOULDBLOCK)) {
            unsuccessful_reads++;
            if (unsuccessful_reads == MAX_SYN_RETRIES) {
                handleClose();
                return -1;
            }
            continue;
        }
        unsuccessful_reads = 0;

        if (length < 0) return length;
        if (get_length(response) != length - HEADER_LEN) continue;
        if (is_corrupt(response)) continue;
        if (get_flags(response) & FIN_BIT) {
            seq_num++;
            handleClose();
            return -1;
        }
        
        // We must get a SYN-ACK before the connection can be considered established
        if ((get_flags(response) & (SYN_BIT|ACK_BIT)) != (SYN_BIT|ACK_BIT)) continue;
        break;
    }

    // Send ACK to server, but it's not important if it is dropped or corrupted
    set_flags(response, ACK_BIT);
    set_seq_num(response, 0);
    set_checksum(response);
    ucpSendTo(ucp_sock, response, HEADER_LEN, &remote_addr);

    destination = remote_addr;
    seq_num++;
    return 0;
}

int RcsConn::recv(void * buf, int maxBytes) {
    // Read data left over from previous recv calls, if possible
    int size = out_queue.pop(buf, maxBytes);
    if (size > 0) {
        return size;
    }
    if (closed) return -1;

    // Additional data is needed, enter main loop

    ucpSetSockRecvTimeout(ucp_sock, RECV_TIMEOUT);

    sockaddr_in addr;
    unsigned char* request = NULL;

    unsigned int unsuccessful_reads = 0;
    while(1) {
        if (!request) {
            // We only have one shot at reading an incoming packet, so we must
            // allocate enough space for a packet of any acceptable length
            request = new unsigned char[MSS];
            if (!request) return -1;
        }

        errno = 0;
        int amount_recvd = ucpRecvFrom(ucp_sock, request, MSS, &addr);
        if ((errno == EAGAIN) || (errno == EWOULDBLOCK)) {
            unsuccessful_reads++;
            if (unsuccessful_reads > MAX_RECV_RETRIES) {
                handleClose();
                if (request) delete[] request;
                return -1;
            }
            continue;
        }
        unsuccessful_reads = 0;

        if (amount_recvd < 0) {
            if (request) delete[] request;
            return amount_recvd;
        }

        bool badPacket = get_length(request) != amount_recvd - HEADER_LEN || is_corrupt(request);
        bool isExpectedPacket = false;
        bool isFinPacket = false;

        if (!badPacket) {
            unsigned short flags = get_flags(request);
            unsigned short request_seq_num = get_seq_num(request);
            if (!(flags & (SYN_BIT|ACK_BIT)) && request_seq_num >= seq_num) {
                isExpectedPacket = (request_seq_num == seq_num) && !(flags & SEND_BIT);
                isFinPacket = (flags & FIN_BIT) != 0;

                // Push this packet into the outbound data buffer
                // (NOTE: this method will also increment seq_num
                // to the value of the next equected sequence number)
                out_queue.push(request, seq_num);
                request = NULL;
            }
        }

        if (isFinPacket) {
            handleClose();
            return -1;
        } else {
            // Send ACK with current sequence number
            unsigned char response[HEADER_LEN] = {0};
            set_flags(response, ACK_BIT);
            set_seq_num(response, seq_num);
            set_checksum(response);
            ucpSendTo(ucp_sock, response, HEADER_LEN, &addr);
        }

        // Break only if we have gotten the packet we expected
        if (badPacket || !isExpectedPacket) continue;
        break;
    }
    return recv(buf, maxBytes);
}

int RcsConn::send(const void * buf, int numBytes) {
    if (closed) return -1;

    ucpSetSockRecvTimeout(ucp_sock, SEND_TIMEOUT);

    int bytesSent = -1;

    // Tell the other side that we want to begin a send, which gives the other side
    // the chance to go into recv mode (if it was previously in send mode or something else)
    // (this packet is denoted by a set SEND bit)
    unsigned char firstPacket[HEADER_LEN] = {0};
    set_flags(firstPacket, SEND_BIT);
    set_seq_num(firstPacket, seq_num);
    set_checksum(firstPacket);
    bool sentFirstPacket = false;

    // Chunk and queue the data if it is too big to fit into one packet
    const unsigned char * charBuf = (const unsigned char *) buf;
    unsigned int packet_seq_num = seq_num + 1;
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
        if (!sentFirstPacket) {
            // Don't start sending packets unless we have sent our first SEND packet
            ucpSendTo(ucp_sock, firstPacket, HEADER_LEN, &destination);
        } else {
            while (next_seq_num < seq_num + WINDOW_SIZE) {
                // Send all available packets in our current window
                try {
                    ucpSendTo(ucp_sock, queue.at(next_seq_num - seq_num), get_length(queue.at(next_seq_num - seq_num)) + HEADER_LEN, &destination);
                } catch (const std::out_of_range& oor) {
                    break;
                }
                next_seq_num++;
            }
        }

        // Response will be an ACK
        unsigned char response[HEADER_LEN] = {0};
        while (1) {
            errno = 0;
            int length = ucpRecvFrom(ucp_sock, response, HEADER_LEN, &destination);
            if ((errno == EAGAIN) || (errno == EWOULDBLOCK)) {
                unsuccessful_reads++;
                if (unsuccessful_reads == MAX_SEND_RETRIES) {
                    handleClose();
                    return bytesSent;
                }
                next_seq_num = get_seq_num(queue.front());
                break;
            }
            unsuccessful_reads = 0;

            if (length < 0) return bytesSent;
            if (get_length(response) != length - HEADER_LEN) break;
            if (is_corrupt(response)) break;

            unsigned short flags = get_flags(response);
            if (flags != ACK_BIT && flags != SEND_BIT && flags != FIN_BIT) {
                // If we're here, we probably got an unacknowledged SYN-ACK
                set_flags(response, ACK_BIT);
                set_seq_num(response, seq_num);
                set_length(response, 0);
                set_checksum(response);
                ucpSendTo(ucp_sock, response, HEADER_LEN, &destination);
                break;
            }

            // Disregard duplicate ACKS, and let the timeout take care of negative ACKs
            if (get_seq_num(response) <= seq_num) continue;
            seq_num = get_seq_num(response);

            // Now we've had at least one packet acknowledged
            if (!sentFirstPacket) {
                bytesSent = 0;
                sentFirstPacket = true;
            }

            // Pop off and delete all queued packets that were cumulatively ACKed
            while (!queue.empty() && get_seq_num(queue.front()) < seq_num) {
                bytesSent += get_length(queue.front());
                delete[] queue.front();
                queue.pop_front();
            }

            // Other side is initiating a send, we should return from our send
            if (get_flags(response) & SEND_BIT) return bytesSent;
            if (get_flags(response) & FIN_BIT) {
                handleClose();
                return bytesSent;
            }
            break;
        }
    }

    return bytesSent;
}

int RcsConn::close() {
    if (closed) return 0;

    // In order to properly close a connection without leaving the other side hanging,
    // close() requires a handshake as well

    ucpSetSockRecvTimeout(ucp_sock, CLOSE_TIMEOUT);

    // Request is a FIN packet (with sequence number equal to seq_num)
    // Response will be an ACK packet (with sequence number probably equal to seq_num + 1)
    unsigned char request[HEADER_LEN] = {0};
    unsigned char response[HEADER_LEN] = {0};

    set_flags(request, FIN_BIT);
    set_seq_num(request, seq_num);
    set_checksum(request);

    for(int i = 0; i < MAX_CLOSE_RETRIES; i++) {
        ucpSendTo(ucp_sock, request, HEADER_LEN, &destination);

        errno = 0;
        int length = ucpRecvFrom(ucp_sock, response, HEADER_LEN, &destination);
        if ((errno == EAGAIN) || (errno == EWOULDBLOCK)) continue;
        if (length < 0) break;
        if (get_length(response) != length - HEADER_LEN) continue;
        if (is_corrupt(response)) continue;
        if (get_seq_num(response) <= seq_num) continue;
        if ((get_flags(response) & ACK_BIT) != ACK_BIT) continue;
        break;
    }

    closed = true;
    return ucpClose(ucp_sock);
}

