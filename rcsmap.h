#include <map>
#include <queue>
#include <utility>

class RcsConn {
    unsigned int ucp_sock;
    sockaddr_in destination;
    std::queue<char *> queue;

    public:
        RcsConn();
        ~RcsConn();
        int bind(sockaddr_in * addr);
        int getSockName(sockaddr_in * addr);
        int listen();
        int accept(sockaddr_in * addr);
        int connect(const sockaddr_in * addr);
        int recv(void * buf, int maxBytes);
        int send(const void * buf, int numBytes);
        int close();
        int getSocketID();
};

class RcsMap {
    std::map<unsigned int, RcsConn> map;
    unsigned int nextId;

    public:
        class NotFound: public Exception {};
        RcsMap();
        ~RcsMap();
        RcsConn & get(unsigned int sockId);
        std::pair<unsigned int, RcsConn &> newConn();
        int close(unsigned int sockId);
};

