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
            waitForMasterTimer,
            monitoringMasterTimer,
            iAmAliveHeartbeetTimer;

public:
    int init(struct NetworkingConfig *netConfig);
    int run();

private:
    int becomeWithoutMaster();
    int becomeMaster();
    int becomeSlave(struct NodeDescriptor *master);

    int sendMessage(enum MessageType type, sockaddr_in *peerAddress);
    int broadcastMessage(enum MessageType type);

    int compareWithSelf(struct NodeIdentity *senderId);
    int compareWithCurrentMaster(struct NodeIdentity *senderId);

    static void recvDgramHandler(struct sockaddr_in* senderAddress, unsigned char *message, size_t messageSize, void *arg);

    void onMessageReceived(enum MessageType type, struct NodeDescriptor *sender);

    int initTimers();

    static void whoIsMasterTimeoutHandler(TimerHandlerArgument arg);
    static void monitoringMasterTimeoutHandler(TimerHandlerArgument arg);
    static void waitForMasterTimeoutHandler(TimerHandlerArgument arg);
    static void iAmAliveHeartbeetTimeoutHandler(TimerHandlerArgument arg);

    void onWhoIsMasterTimeout();
    void onMonitoringMasterTimeout();
    void onWaitForMasterTimeout();
    void onIAmAliveHeartbeetTimeout();
};

#endif // NODES_H
