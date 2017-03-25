#ifndef NODES_H
#define NODES_H

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

#include "timers.h"
#include "networking.h"

enum NodeState
{
    WithoutMaster,
    SubscribingToMaster,
    WhantToBeMaster,
    Master,
    Slave
};

enum MessageType
{
    WhoIsMaster,
    IAmMaster,
    IWantToBeMaster,
    IAmGreaterThanYou,

    IAmYourMaster,
    IAmYourSlave,

    PingMaster,
    PongMaster
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

    NodeID myMaster;
    bool masterIsAvailable;

    struct Networking net;

    int init(struct NetworkingConfig *netConfig);


    NodeID getUniqueNodeId();

    void run();

    void becameWithoutMaster();

    void sendMessage(enum MessageType type, NodeID receiver);
    void broadcastMessage(enum MessageType type);

    bool compareWithSelf(NodeID senderId);

    void onMessageReceived(enum MessageType type, NodeID senderId);

    void onWhoIsMasterTimeout();
    void onSubscribingToMasterTimeout();
    void onWaitForPongMasterTimeout();
    void onMonitoringMasterTimeout();
};

#endif // NODES_H
