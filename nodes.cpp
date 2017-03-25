#include "nodes.h"

int SelfNode::init(NetworkingConfig *netConfig)
{
    memset(&this->net, 0, sizeof(struct Networking));
    if (this->net.init(netConfig) == -1) {
        // TODO: Log error
        return -1;
    }

    this->id = this->getUniqueNodeId();
    this->state = WithoutMaster;

    return 0;
}

NodeID SelfNode::getUniqueNodeId()
{
    // TODO: use network params
}

void recvDgramHandler(struct sockaddr_in* senderAddress, char *message, size_t messageSize, void *arg)
{
    struct SelfNode *self = (struct SelfNode*)arg;

    // TODO: parse message

    self->onMessageReceived();
}

void SelfNode::run()
{
    this->becameWithoutMaster();
    this->net.runRecvLoop(recvDgramHandler, (void*)this);
}

void SelfNode::becameWithoutMaster()
{
    this->state = WithoutMaster;
    this->broadcastMessage(WhoIsMaster);
    this->startTimer_WhoIsMaster();
}

void SelfNode::sendMessage(MessageType type, NodeID receiver)
{

}

void SelfNode::broadcastMessage(MessageType type)
{

}

bool SelfNode::compareWithSelf(NodeID senderId)
{

}

void SelfNode::onMessageReceived(MessageType type, NodeID senderId)
{
    switch (type) {
    case WhoIsMaster:
        if (this->state == Master) {
            this->sendMessage(IAmMaster, senderId);
        }
        break;
    case IAmMaster:
        if (this->state == WithoutMaster) {
            this->state = SubscribingToMaster;
            this->myMaster = senderId;
            this->sendMessage(IAmYourSlave, senderId);
            this->startTimerSubscribingToMaster();
        }
        else if (this->state == Slave && this->myMaster == senderId) {
            this->startMonitoringMasterTimer();
        }
        break;
    case IWantToBeMaster:
        switch (this->state) {
        case Master:
            this->sendMessage(IAmMaster, senderId);
            break;
        case Slave:

            break;
        case WhantToBeMaster:
            if (this->compareWithSelf(senderId)) {
                this->sendMessage(IAmGreaterThanYou, senderId);
            }
            break;
        default:
            break;
        }
        break;
    case IAmYourSlave:
        if (this->state == Master) {
            this->sendMessage(IAmYourMaster, senderId);
        }
        break;
    case IAmYourMaster:
        if (this->state == SubscribingToMaster && this->myMaster == senderId) {
            this->state = Slave;
            this->sendMessage(PingMaster, this->myMaster);
            this->startWaitForPongMasterTimer();
        }
        break;
    case PingMaster:
        if (this->state == Master) {
            this->sendMessage(PongMaster, senderId);
        }
        break;
    case PongMaster:
        if (this->state == Slave) {
            this->startMonitoringMasterTimer();
        }
        break;
    default:
        break;
    }
}

void SelfNode::onWhoIsMasterTimeout()
{
    this->state = WhantToBeMaster;
    this->broadcastMessage(IWantToBeMaster);
}

void SelfNode::onSubscribingToMasterTimeout()
{
    this->becameWithoutMaster();
}

void SelfNode::onWaitForPongMasterTimeout()
{
    this->becameWithoutMaster();
}

void SelfNode::onMonitoringMasterTimeout()
{
    if (this->state == Slave) {
        this->sendMessage(PingMaster, this->myMaster);
        this->startWaitForPongMasterTimer();
    }
}
