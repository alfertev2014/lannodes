#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

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


int bindDgramSocket(struct sockaddr_in *addr)
{
    int socketFd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

    if (socketFd < 0)
    {
        // TODO: Log error
        return -1;
    }

    int broadcastSocketOption = 1;
    int err = setsockopt(socketFd,
        SOL_SOCKET, SO_BROADCAST,
        &broadcastSocketOption, sizeof(broadcastSocketOption));
    if (err < 0)
    {
        // TODO: Log error
        return -1;
    }


    if (bind(socketFd, (struct sockaddr*)&addr, sizeof(struct sockaddr_in)) < 0)
    {
        // TODO: Log error
        return -1;
    }

    // success
    return socketFd;
}

void initSockedAddress(struct sockaddr_in *addr, uint32_t ipAddress, uint16_t port)
{
    addr->sin_family = AF_INET;
    addr->sin_port = htonl(port);
    addr->sin_addr.s_addr = htonl(ipAddress);
}

struct Networking
{
    struct sockaddr_in selfDgramAddress;
    struct sockaddr_in broadcastDgramAddress;
    int dgramSocketFd;

    int init(struct NetworkingConfig *config) {


        initSockedAddress(&this->selfDgramAddress, INADDR_ANY, config->udpPort);
        initSockedAddress(&this->broadcastDgramAddress, INADDR_BROADCAST, config->udpPort);

        int socketFd = bindDgramSocket(&this->selfDgramAddress);
        if (socketFd < 0)
        {
            // TODO: Log error
            return -1;
        }

        this->dgramSocketFd = socketFd;
    }

    void deinit()
    {
        if (this->dgramSocketFd > 0)
        {
            if (close(this->dgramSocketFd) < 0)
            {
                // TODO: Log error
                return;
            }
            this->dgramSocketFd = 0;
        }
    }


    int broadcastDgram(char *content, size_t contentSize)
    {
        return sendto(this->dgramSocketFd,
            content, contentSize,
            0,
            (struct sockaddr*)&this->broadcastDgramAddress, sizeof(struct sockaddr_in));
    }
};


enum NodeState
{
    WithoutMaster,
    SubscribingToMaster,
    Master,
    Slave
};

enum MessageType
{
    WhoIsMaster,
    IAmMaster,
    IAmYourMaster,
    IAmYourSlave,
};

typedef uint32_t NodeID;

struct NodeDescriptor
{
    struct sockaddr_in peerAddress;
    NodeID id;

    void setAddress(uint32_t ipAddress, uint16_t port)
    {
        memset(&this->peerAddress, 0, sizeof(struct sockaddr_in));
        initSockedAddress(&this->peerAddress, ipAddress, port);
    }
};

struct SelfNode
{
    NodeID id;
    enum NodeState state;

    struct Networking net;

    void init(struct NetworkingConfig *netConfig)
    {
        memset(&this->net, 0, sizeof(struct Networking));
        this->net.init(netConfig);

        // TODO: this->id = getUniqueNodeId();
        this->state = WithoutMaster;
    }
};

int main(int argc, char * argv[])
{

}
