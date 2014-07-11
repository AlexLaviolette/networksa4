#include <map>
#include <queue>
#include <utility>
#include <netinet/in.h>

class RcsConn {
    unsigned int ucp_sock;
    sockaddr_in destination;
    std::queue<char *> queue;
    unsigned int seq_num;

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

    private:
        bool is_corrupt(const char * packet);
        unsigned char get_flags(const char * packet);
        unsigned short get_length(const char * packet);
        unsigned short get_seq_num(const char * packet);
        unsigned short calculate_checksum(const char * packet);
        void set_flags(char * packet, unsigned char flags);
        void set_length(char * packet, unsigned short length);
        void set_seq_num(char * packet, unsigned short seq_num);
        void set_checksum(char * packet);
};

class RcsMap {
    std::map<unsigned int, RcsConn> map;
    unsigned int nextId;

    public:
        class NotFound: public std::exception {};
        RcsMap();
        ~RcsMap();
        RcsConn & get(unsigned int sockId);
        std::pair<unsigned int, RcsConn &> newConn();
        int close(unsigned int sockId);
};

