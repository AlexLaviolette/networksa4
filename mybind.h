#ifndef MYBIND_H
#define MYBIND_H

#include <netinet/in.h>

#ifdef __cplusplus
extern "C" {
#endif

int mybind(int sockfd, struct sockaddr_in *addr);

#ifdef __cplusplus
}
#endif

#endif
