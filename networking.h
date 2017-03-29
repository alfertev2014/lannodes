#ifndef NETWORKING_H
#define NETWORKING_H

#include <netinet/in.h>
#include <netinet/ip.h>

struct NetworkingConfig
{
    uint16_t udpPort;
};

typedef void (*RecvHandler)(struct sockaddr_in* senderAddress, unsigned char *message, size_t messageSize, void *arg);

struct Networking
{
    struct sockaddr_in recvDgramAddress;
    struct sockaddr_in broadcastDgramAddress;
    int dgramSocketFd;

    bool breakRecvLoop;

    int init(struct NetworkingConfig *config);
    int deinit();

    int broadcastDgram(unsigned char *content, size_t contentSize);
    int sendDgram(sockaddr_in *peerAddress, unsigned char *content, size_t contentSize);
    int runRecvLoop(RecvHandler handler, void *arg);
};


#endif // NETWORKING_H
