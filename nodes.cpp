#include "nodes.h"

#include <string.h>

int SelfNode::init(NetworkingConfig *netConfig)
{
    memset(&this->net, 0, sizeof(struct Networking));
    if (this->net.init(netConfig) == -1) {
        // TODO: Log error
        return -1;
    }

    memset(&this->nodeIdentity, 0, sizeof(struct NodeIdentity));

    if (getSelfNodeIdentity(&this->nodeIdentity) == -1) {
        // TODO: Log error
        return -1;
    }

    this->state = WithoutMaster;

    return 0;
}

size_t deserializeNodeIdentity(char *buffer, struct NodeIdentity *id)
{
    size_t bytesRead = 0;
    id->processId = ntohl(((uint32_t*)buffer)[0]);
    bytesRead += sizeof(uint32_t);
    buffer += bytesRead;
    memcpy(id->macAddress, buffer, 6);
    bytesRead += 6;
    return bytesRead;
}

size_t serializeNodeIdentity(struct NodeIdentity *id, char *buffer)
{
    size_t bytesWritten = 0;

    ((uint32_t*)buffer)[0] = htonl(id->processId);

    bytesWritten += sizeof(uint32_t);
    buffer += bytesWritten;

    memcpy(buffer, id->macAddress, 6);

    bytesWritten += 6;

    return bytesWritten;
}

size_t serializeMessage(MessageType type, struct NodeIdentity *nodeId, char *buffer)
{
    char *bufferBegin = buffer;

    ((uint32_t*)buffer)[0] = htonl(type);
    buffer += sizeof(uint32_t);

    size_t size = serializeNodeIdentity(nodeId, buffer);
    buffer += size;

    return (size_t)(buffer - bufferBegin);
}

size_t deserializeMessage(char *buffer, MessageType *type, struct NodeIdentity *nodeId)
{
    char *bufferBegin = buffer;
    *type = (MessageType)ntohl(((uint32_t*)buffer)[0]);
    buffer += sizeof(uint32_t);

    size_t size = deserializeNodeIdentity(buffer, nodeId);
    buffer += size;

    return (size_t)(buffer - bufferBegin);
}

void recvDgramHandler(struct sockaddr_in* senderAddress, char *message, size_t messageSize, void *arg)
{
    struct SelfNode *self = (struct SelfNode*)arg;

    struct NodeDescriptor senderNode;
    senderNode.peerAddress = *senderAddress;

    MessageType type;
    deserializeMessage(message, &type, &senderNode.id);

    if (self->compareWithSelf(&senderNode.id) != 0) {
        self->onMessageReceived(type, &senderNode);
    }
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
    this->whoIsMasterTimer.start();
}

#define MESSAGE_BUFFER_SIZE 8192
char sendMessageBuffer[MESSAGE_BUFFER_SIZE];

int SelfNode::sendMessage(MessageType type, NodeDescriptor *senderId)
{
    size_t size = serializeMessage(type, &this->nodeIdentity, sendMessageBuffer);

    if (this->net.sendDgram(&senderId->peerAddress, sendMessageBuffer, size) == -1) {
        // TODO: Log error
        return -1;
    }

    return 0;
}

int SelfNode::broadcastMessage(MessageType type)
{
    size_t size = serializeMessage(type, &this->nodeIdentity, sendMessageBuffer);

    if (this->net.broadcastDgram(sendMessageBuffer, size) == -1) {
        // TODO: Log error
        return -1;
    }

    return 0;
}

int SelfNode::compareWithSelf(NodeIdentity *senderId)
{
    return compareNodeIdentities(&this->nodeIdentity, senderId);
}

void SelfNode::onMessageReceived(MessageType type, struct NodeDescriptor *senderId)
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
            this->myMaster = *senderId;
            this->sendMessage(IAmYourSlave, senderId);
            this->subscribingToMasterTimer.start();
        }
        else if (this->state == Slave &&
                this->compareWithSelf(&senderId->id) == 0) {
            this->monitoringMasterTimer.start();
        }
        break;
    case IWantToBeMaster:
        switch (this->state) {
        case Master:
            this->sendMessage(IAmMaster, senderId);
            break;
        case Slave:

            break;
        case WhantToBeMaster: {
            int cmp = this->compareWithSelf(&senderId->id);
            if (cmp > 0) {
                this->sendMessage(IAmGreaterThanYou, senderId);
            }
            else if (cmp < 0) {

            }
            break;
        }
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
        if (this->state == SubscribingToMaster &&
            compareNodeIdentities(&this->myMaster.id, &senderId->id) == 0) {
            this->state = Slave;
            this->sendMessage(PingMaster, &this->myMaster);
            this->waitForPongMasterTimer.start();
        }
        break;
    case PingMaster:
        if (this->state == Master) {
            this->sendMessage(PongMaster, senderId);
        }
        break;
    case PongMaster:
        if (this->state == Slave) {
            this->monitoringMasterTimer.start();
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
        this->sendMessage(PingMaster, &this->myMaster);
        this->waitForPongMasterTimer.start();
    }
}

static void whoIsMasterTimeoutHandler(TimerHandlerArgument arg)
{
    if (arg.ptrValue)
        ((SelfNode*)arg.ptrValue)->onWhoIsMasterTimeout();
}

static void subscribingToMasterTimeoutHandler(TimerHandlerArgument arg)
{
    if (arg.ptrValue)
        ((SelfNode*)arg.ptrValue)->onSubscribingToMasterTimeout();
}

static void waitForPongMasterTimeoutHandler(TimerHandlerArgument arg)
{
    if (arg.ptrValue)
        ((SelfNode*)arg.ptrValue)->onWaitForPongMasterTimeout();
}

static void monitoringMasterTimeoutHandler(TimerHandlerArgument arg)
{
    if (arg.ptrValue)
        ((SelfNode*)arg.ptrValue)->onMonitoringMasterTimeout();
}

int SelfNode::initTimers()
{
    TimerHandlerArgument arg;
    arg.ptrValue = (void*)this;
    this->whoIsMasterTimer.init(1000, false, whoIsMasterTimeoutHandler, arg);
    this->subscribingToMasterTimer.init(1000, false, subscribingToMasterTimeoutHandler, arg);
    this->waitForPongMasterTimer.init(1000, false, waitForPongMasterTimeoutHandler, arg);
    this->monitoringMasterTimer.init(1000, false, monitoringMasterTimeoutHandler, arg);
}

