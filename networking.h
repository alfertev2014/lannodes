#ifndef NETWORKING_H
#define NETWORKING_H

// int types
#include <stdint.h>

// memset
#include <string.h>

#include <sys/types.h>
#include <ifaddrs.h>

// socket, ip
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>

// close fd
#include <unistd.h>

struct NetworkingConfig
{
    uint16_t udpPort;
};

struct Networking
{
    struct sockaddr_in selfDgramAddress;
    struct sockaddr_in broadcastDgramAddress;
    int dgramSocketFd;

    int init(struct NetworkingConfig *config);
    void deinit();

    int broadcastDgram(char *content, size_t contentSize);
};


#endif // NETWORKING_H
