#ifndef NODES_H
#define NODES_H

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

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

    void init(struct NetworkingConfig *netConfig)
    {
        memset(&this->net, 0, sizeof(struct Networking));
        this->net.init(netConfig);

        this->id = getUniqueNodeId();
        this->state = WithoutMaster;
    }

    NodeID getUniqueNodeId()
    {
        // TODO: use network params
    }

    void run()
    {
        becameWithoutMaster();
    }

    void becameWithoutMaster()
    {
        this->state = WithoutMaster;
        broadcastMessage(WhoIsMaster);
        startTimer_WhoIsMaster();
    }

    void sendMessage(enum MessageType type, NodeID receiver)
    {

    }

    void broadcastMessage(enum MessageType type)
    {

    }

    bool compareWithSelf(NodeID senderId)
    {

    }

    void onMessageReceived(enum MessageType type, NodeID senderId)
    {
        switch (type) {
        case WhoIsMaster:
            if (this->state == Master) {
                sendMessage(IAmMaster, senderId);
            }
            break;
        case IAmMaster:
            if (this->state == WithoutMaster) {
                this->state = SubscribingToMaster;
                this->myMaster = senderId;
                sendMessage(IAmYourSlave, senderId);
                startTimerSubscribingToMaster();
            }
            else if (this->state == Slave && this->myMaster == senderId) {
                startMonitoringMasterTimer();
            }
            break;
        case IWantToBeMaster:
            switch (this->state) {
            case Master:
                sendMessage(IAmMaster, senderId);
                break;
            case Slave:

                break;
            case WhantToBeMaster:
                if (compareWithSelf(senderId)) {
                    sendMessage(IAmGreaterThanYou, senderId);
                }
                break;
            default:
                break;
            }
            break;
        case IAmYourSlave:
            if (this->state == Master) {
                sendMessage(IAmYourMaster, senderId);
            }
            break;
        case IAmYourMaster:
            if (this->state == SubscribingToMaster && this->myMaster == senderId) {
                this->state = Slave;
                sendMessage(PingMaster, this->myMaster);
                startWaitForPongMasterTimer();
            }
            break;
        case PingMaster:
            if (this->state == Master) {
                sendMessage(PongMaster, senderId);
            }
            break;
        case PongMaster:
            if (this->state == Slave) {
                startMonitoringMasterTimer();
            }
            break;
        default:
            break;
        }
    }

    void onWhoIsMasterTimeout()
    {
        this->state = WhantToBeMaster;
        broadcastMessage(IWantToBeMaster);
    }

    void onSubscribingToMasterTimeout()
    {
        becameWithoutMaster();
    }

    void onWaitForPongMasterTimeout()
    {
        becameWithoutMaster();
    }

    void onMonitoringMasterTimeout()
    {
        if (this->state == Slave) {
            sendMessage(PingMaster, this->myMaster);
            startWaitForPongMasterTimer();
        }
    }
};

#endif // NODES_H
