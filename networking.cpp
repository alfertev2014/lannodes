#include "networking.h"

// int types
#include <stdint.h>

// memset
#include <string.h>

// socket, ip
#include <sys/socket.h>


// close fd
#include <unistd.h>

static int bindDgramSocket(sockaddr_in *addr)
{
    int socketFd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

    if (socketFd < 0) {
        // TODO: Log error
        return -1;
    }

    int broadcastSocketOption = 1;

    if (setsockopt(socketFd,
            SOL_SOCKET, SO_BROADCAST,
            &broadcastSocketOption, sizeof(broadcastSocketOption)) == -1) {
        // TODO: Log error
        return -1;
    }


    if (bind(socketFd, (struct sockaddr*)&addr, sizeof(struct sockaddr_in)) == -1) {
        // TODO: Log error
        return -1;
    }

    // success
    return socketFd;
}

static void initSocketAddress(sockaddr_in *addr, uint32_t ipAddress, uint16_t port)
{
    addr->sin_family = AF_INET;
    addr->sin_port = htons(port);
    addr->sin_addr.s_addr = htonl(ipAddress);
}

int Networking::init(NetworkingConfig *config) {
    this->breakRecvLoop = false;

    initSocketAddress(&this->recvDgramAddress, INADDR_ANY, config->udpPort);
    initSocketAddress(&this->broadcastDgramAddress, INADDR_BROADCAST, config->udpPort);

    int socketFd = bindDgramSocket(&this->recvDgramAddress);
    if (socketFd == -1) {
        // TODO: Log error
        return -1;
    }

    this->dgramSocketFd = socketFd;
    return 0;
}

int Networking::deinit()
{
    if (this->dgramSocketFd != -1)
    {
        if (close(this->dgramSocketFd) == -1)
        {
            // TODO: Log error
            return -1;
        }
        this->dgramSocketFd = -1;
    }
    return 0;
}

int Networking::broadcastDgram(char *content, size_t contentSize)
{
    ssize_t sizeBeSent =
        sendto(this->dgramSocketFd,
            content, contentSize,
            0,
            (struct sockaddr*)&this->broadcastDgramAddress, sizeof(struct sockaddr_in));

    if (sizeBeSent < 0) {
        // TODO: Log error
    }

    return sizeSent;
}

#define RECV_BUFFER_SIZE 8196

char recvBuffer[RECV_BUFFER_SIZE];

int Networking::runRecvLoop(RecvHandler handler, void *arg)
{
    if (this->dgramSocketFd < 0) {
        // TODO: Log error
        return -1;
    }

    this->breakRecvLoop = false;

    while (!this->breakRecvLoop) {
        struct sockaddr_in senderAddress;
        socklen_t addressLength = sizeof(struct sockaddr_in);

        ssize_t sizeBeRecieved =
            recvfrom(this->dgramSocketFd, recvBuffer, RECV_BUFFER_SIZE,
                0,
                (struct sockaddr*)&senderAddress, &addressLength);

        if (sizeBeRecieved < 0) {
            // TODO: Log error
            continue;
        }

        handler(senderAddress, recvBuffer, sizeBeRecieved, arg);
    }
}
