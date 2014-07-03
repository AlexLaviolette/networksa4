#include <map>
#include <queue>

class RcsConn {
    unsigned int ucp_sock;
    sockaddr_in destination;
    std::queue<char *> queue;

    public:
        RcsConn();
        ~RcsConn();
        int bind(const sockaddr_in * addr);
        int getSockName(sockaddr_in * addr);
        int listen();
        int accept();
        int connect(const sockaddr_in * addr);
        int recv(void * buf, int maxBytes);
        int send(const void * buf, int numBytes);
        int getSocketID();
};

class RcsMap {
    std::map<unsigned int, RcsConn> map;
    unsigned int nextId;

    public:
        RcsMap();
        ~RcsMap();
        RcsConn & get(unsigned int sockId);
        RcsConn & newConn();
};
