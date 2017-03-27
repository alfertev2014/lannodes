#ifndef NODES_H
#define NODES_H

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

#include "timers.h"
#include "networking.h"
#include "identity.h"

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


struct NodeDescriptor
{
    struct sockaddr_in peerAddress;
    struct NodeIdentity id;
};

struct SelfNode
{
    struct NodeIdentity nodeIdentity;

    enum NodeState state;

    struct NodeDescriptor myMaster;
    bool masterIsAvailable;

    struct Networking net;

    int init(struct NetworkingConfig *netConfig);

    void run();

    void becameWithoutMaster();

    int sendMessage(enum MessageType type, struct NodeDescriptor *senderId);
    int broadcastMessage(enum MessageType type);

    int compareWithSelf(NodeID senderId);

    void onMessageReceived(enum MessageType type, struct NodeDescriptor *senderId);

    void onWhoIsMasterTimeout();
    void onSubscribingToMasterTimeout();
    void onWaitForPongMasterTimeout();
    void onMonitoringMasterTimeout();
};

#endif // NODES_H
