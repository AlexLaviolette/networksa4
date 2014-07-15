#ifndef UCP_H
#define UCP_H

#include <netinet/in.h>

#ifdef __cplusplus
extern "C" {
#endif

int ucpSocket();
int ucpBind(int sockfd, struct sockaddr_in *addr);
int ucpGetSockName(int sockfd, struct sockaddr_in *addr);
int ucpSetSockRecvTimeout(int sockfd, int milliSecs);
int ucpSendTo(int sockfd, const void *buf, int len, const struct sockaddr_in *to);
int ucpRecvFrom(int sockfd, void *buf, int len, struct sockaddr_in *from);
int ucpClose(int sockfd);

#ifdef __cplusplus
}
#endif

#endif
