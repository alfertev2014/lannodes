#include "networking.h"

#include <stdio.h>
#include <string.h>

// int types
#include <stdint.h>

// socket, ip
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <unistd.h>

#include <errno.h>

#include "timers.h"
#include "logging.h"

static int bindDgramSocket(struct sockaddr_in *addr)
{
    int socketFd = socket(AF_INET, SOCK_DGRAM | SOCK_NONBLOCK, 0);

    if (socketFd < 0) {
        perror("Bind dgram socket");
        logPosition();
        return -1;
    }

    int broadcastSocketOption = 1;

    if (setsockopt(socketFd,
            SOL_SOCKET, SO_BROADCAST,
            &broadcastSocketOption, sizeof(int)) == -1) {
        perror("Set broadcast socket oprion");
        logPosition();
        return -1;
    }

    int reuseAddressSocketOption = 1;
    if (setsockopt(socketFd,
            SOL_SOCKET, SO_REUSEADDR,
            &reuseAddressSocketOption, sizeof(int)) == -1) {
        perror("Set reuse address socket option");
        logPosition();
        return 1;
    }


    if (bind(socketFd, (struct sockaddr*)addr, sizeof(struct sockaddr_in)) == -1) {
        perror("Bind dgram socket");
        logPosition();
        return -1;
    }

    // success
    return socketFd;
}

static void initSocketAddress(struct sockaddr_in *addr, uint32_t ipAddress, uint16_t port)
{
    addr->sin_family = AF_INET;
    addr->sin_port = htons(port);
    addr->sin_addr.s_addr = htonl(ipAddress);
}

int Networking::init(struct NetworkingConfig *config) {
    this->breakRecvLoop = false;

    initSocketAddress(&this->recvDgramAddress, INADDR_ANY, config->udpPort);
    initSocketAddress(&this->broadcastDgramAddress, INADDR_BROADCAST, config->udpPort);

    int socketFd = bindDgramSocket(&this->recvDgramAddress);
    if (socketFd == -1) {
        logPosition();
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
            logPosition();
            return -1;
        }
        this->dgramSocketFd = -1;
    }
    return 0;
}

int Networking::broadcastDgram(unsigned char *content, size_t contentSize)
{
    ssize_t sizeBeSent =
        sendto(this->dgramSocketFd,
            content, contentSize,
            0,
            (struct sockaddr*)&this->broadcastDgramAddress, sizeof(struct sockaddr_in));

    if (sizeBeSent < 0) {
        perror("Broadcast dgram");
        logPosition();
        return -1;
    }

    return sizeBeSent;
}

int Networking::sendDgram(struct sockaddr_in *peerAddress, unsigned char *content, size_t contentSize)
{
    ssize_t sizeBeSent =
        sendto(this->dgramSocketFd,
            content, contentSize,
            0,
            (struct sockaddr*)peerAddress, sizeof(struct sockaddr_in));

    if (sizeBeSent < 0) {
        perror("Send dgram");
        logPosition();
    }

    return sizeBeSent;
}

#define RECV_BUFFER_SIZE 8196

unsigned char recvBuffer[RECV_BUFFER_SIZE];

int Networking::runRecvLoop(RecvHandler handler, void *arg)
{
    if (this->dgramSocketFd < 0) {
        logPosition();
        return -1;
    }

    this->breakRecvLoop = false;

    sigset_t old_mask;

    Timer::lockTimers(&old_mask);

    while (!this->breakRecvLoop) {
        fd_set fds;
        int res;

        FD_ZERO (&fds);
        FD_SET (this->dgramSocketFd, &fds);
        res = pselect(this->dgramSocketFd + 1, &fds, NULL, NULL, NULL, &old_mask);

        if (res < 0 && errno != EINTR) {
            perror ("select");
            return -1;
        }
        else if (this->breakRecvLoop) {
            break;
        }
        else if (res == 0)
            continue;

        if (FD_ISSET(this->dgramSocketFd, &fds)) {
            struct sockaddr_in senderAddress;
            socklen_t addressLength = sizeof(struct sockaddr_in);

            ssize_t sizeBeRecieved =
                recvfrom(this->dgramSocketFd, recvBuffer, RECV_BUFFER_SIZE,
                    0,
                    (struct sockaddr*)&senderAddress, &addressLength);

            if (sizeBeRecieved < 0) {
                if (errno != EINTR && errno != EAGAIN) {
                    perror("Receive dgram");
                    logPosition();
                }
            }
            else {
                handler(&senderAddress, recvBuffer, sizeBeRecieved, arg);
            }
        }
        Timer::runAllPendingTimouts();
    }
    return 0;
}


