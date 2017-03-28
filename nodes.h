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
    Master,
    Slave
};

enum MessageType
{
    WhoIsMaster,
    IAmMaster,
    PleaseWait,
};


struct NodeDescriptor
{
    struct sockaddr_in peerAddress;
    struct NodeIdentity id;
};

struct SelfNode
{
private:
    struct NodeIdentity nodeIdentity;

    enum NodeState state;

    struct NodeDescriptor myMaster;
    bool masterIsAvailable;

    struct Networking net;

    struct Timer whoIsMasterTimer,
            waitForPongMasterTimer,
            monitoringMasterTimer;

public:
    int init(struct NetworkingConfig *netConfig);
    void run();

private:
    void becameWithoutMaster();

    int sendMessage(enum MessageType type, sockaddr_in *peerAddress);
    int broadcastMessage(enum MessageType type);

    int compareWithSelf(struct NodeIdentity *senderId);
    int compareWithCurrentMaster(struct NodeIdentity *senderId);

    static void recvDgramHandler(struct sockaddr_in* senderAddress, char *message, size_t messageSize, void *arg);

    void onMessageReceived(enum MessageType type, struct NodeDescriptor *sender);

    int initTimers();

    static void whoIsMasterTimeoutHandler(TimerHandlerArgument arg);
    static void monitoringMasterTimeoutHandler(TimerHandlerArgument arg);

    void onWhoIsMasterTimeout();
    void onMonitoringMasterTimeout();
};

#endif // NODES_H
