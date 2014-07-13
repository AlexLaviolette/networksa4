#ifndef RCS_H
#define RCS_H

#include <netinet/in.h>

#ifdef __cplusplus
extern "C" {
#endif

    int rcsSocket();
    int rcsBind(int socketID, struct sockaddr_in * addr);
    int rcsGetSockName(int socketID, struct sockaddr_in * addr);
    int rcsListen(int socketID);
    int rcsAccept(int socketID, struct sockaddr_in * addr);
    int rcsConnect(int socketID, const struct sockaddr_in * addr);
    int rcsRecv(int socketID, void * rcvBuffer, int maxBytes);
    int rcsSend(int socketID, const void * sendBuffer, int numBytes);
    int rcsClose(int socketID);

#ifdef __cplusplus
}
#endif

#endif
