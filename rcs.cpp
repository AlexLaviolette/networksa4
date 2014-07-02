#include <iostream>

using namespace std;

// A server based on your API will invoke rcsSocket(), rcsBind(), rcsListen(), rcsAccept(), rcsRecv(), rcsSend () and rcsClose ().
// A client will invoke rcsSocket (), rcsBind (), rcsConnect (), rcsRecv (), rcsSend() and rcsClose().

//  used to allocate an RCS socket. No arguments. Returns a socket descriptor (positive integer) on success.
int rcsSocket() {
	return ucpSocket();
}

// binds an RCS socket (first argument) to the address structure (second argument). Returns 0 on success.
int rcsBind(int socketID, const struct sockaddr_in * addr) {
    return ucpBind(socketID, addr)
}

// fills in the address information into the second argument with which an RCS socket (first argument)
// has been bound via a call to rcsBind(). Returns 0 on success.
int rcsGetSockName(int socketID, struct sockaddr_in * addr){


	return 0;
}

// marks an RCS socket (the argument) as listening for connection requests. Returns 0 on success.
int rcsListen(int socketID) {


	return 0;
}

// accepts a connection request on a socket (the first argument). This is a blocking call while awaiting
// connection requests. The call is unblocked when a connection request is received. The address of the
// peer (client) is filled into the second argument. The call returns a descriptor to a new RCS socket
// that can be used to rcsSend () and rcsRecv () with the peer (client).
int rcsAccept(int socketID, struct sockaddr_in * addr) {

}

// connects a client to a server. The socket (first argu- ment) must have been bound beforehand using rcsBind().
// The second argument identifies the server to which connection should be attempted. Returns 0 on success.
int rcsConnect(int socketID, const struct sockaddr_in * addr) {


	return 0;
}

// blocks awaiting data on a socket (first argument). Presumably, the socket is one that has been returned by a
// prior call to rcsAccept(), or on which rcsConnect() has been suc- cessfully called. The second argument is
// the buffer which is filled with received data. The maximum amount of data that may be written is identified
// by the third argument. Returns the actual amount of data received. “Amount” is the number of bytes. Data is
// sent and received reliably, so any byte that is returned by this call should be what was sent, and in the correct order.
int rcsRecv(int socketID, void * rcvBuffer; int maxBytes) {


}

// blocks sending data. The first argument is a socket descriptor that has been returned by a prior call to rcsAccept(),
// or on which rcsConnect() has been successfully called. The second argument is the buffer that contains the data to be sent.
// The third argument is the number of bytes to be sent. Returns the actual number of bytes sent. If rcsSend() returns with
// a non-negative return value, then we know that so many bytes were reliably received by the other end.
int rcsSend(int socketID, const void * sendBuffer; int numBytes) {


}

// closes an RCS socket descriptor. Returns 0 on success.
int rcsClose(int socketID) {


	return 0;
}
