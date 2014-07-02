#include <iostream>

using namespace std;


int rcsSocket() {
	return ucpSocket();
}


int rcsBind(int socketID, const struct sockaddr_in * addr) {
    return ucpBind(socketID, addr)
}



int rcsGetSockName(int socketID, struct sockaddr_in * addr){


	return 0;
}


int rcsListen(int socketID) {


	return 0;
}


int rcsAccept(int socketID, struct sockaddr_in * addr) {

}


int rcsConnect(int socketID, const struct sockaddr_in * addr) {


	return 0;
}


int rcsRecv(int socketID, void * rcvBuffer; int maxBytes) {


}





int rcsSend(int socketID, const void * sendBuffer; int numBytes) {


}


int rcsClose(int socketID) {


	return 0;
}
