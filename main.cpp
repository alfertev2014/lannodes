#include "main.h"

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

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
