#include "nodes.h"

#include <string.h>

#include "logging.h"

#include <arpa/inet.h>

static void printNodeDescriptor(struct NodeDescriptor *node);
static void printNodeIdentity(struct NodeIdentity *nodeId);

int SelfNode::init(NetworkingConfig *netConfig)
{
    memset(&this->net, 0, sizeof(struct Networking));
    if (this->net.init(netConfig) == -1) {
        logPosition();
        return -1;
    }

    memset(&this->nodeIdentity, 0, sizeof(struct NodeIdentity));

    if (NodeIdentity::getSelfNodeIdentity(&this->nodeIdentity) == -1) {
        logPosition();
        return -1;
    }

    if (this->initTimers() == -1) {
        logPosition();
        return -1;
    }

    this->state = WithoutMaster;
    this->clientsCount = 0;

    return 0;
}

struct WriteByteStream {
    unsigned char *buffer;
    size_t bufferSize;

    void openStream(unsigned char *buffer, size_t bufferSize)
    {
        this->buffer = buffer;
        this->bufferSize = bufferSize;
    }

    int writeInt32(uint32_t value)
    {
        if (bufferSize < sizeof(uint32_t))
            return -1;
        ((uint32_t*)buffer)[0] = htonl(value);
        buffer += sizeof(uint32_t);
        bufferSize -= sizeof(uint32_t);
        return 0;
    }

    int writeBytes(const unsigned char *bytes, size_t bytesCount)
    {
        if (bufferSize < bytesCount)
            return -1;
        memcpy(buffer, bytes, bytesCount);
        buffer += bytesCount;
        bufferSize -= bytesCount;
        return 0;
    }
};

struct ReadByteStream {
    unsigned char *buffer;
    size_t bufferSize;

    void openStream(unsigned char *buffer, size_t bufferSize)
    {
        this->buffer = buffer;
        this->bufferSize = bufferSize;
    }

    int readInt32(uint32_t *value)
    {
        if (bufferSize < sizeof(uint32_t))
            return -1;
        *value = ntohl(((uint32_t*)buffer)[0]);
        buffer += sizeof(uint32_t);
        bufferSize -= sizeof(uint32_t);
        return 0;
    }

    int readBytes(unsigned char *bytes, size_t bytesCount)
    {
        if (bufferSize < bytesCount)
            return -1;
        memcpy(bytes, buffer, bytesCount);
        buffer += bytesCount;
        bufferSize -= bytesCount;
        return 0;
    }
};

int serializeNodeIdentity(struct WriteByteStream *s, struct NodeIdentity *id)
{
    if (s->writeInt32(id->processId) == -1)
        return -1;

    if (s->writeBytes(id->macAddress, 6) == -1)
        return -1;

    return 0;
}

int serializeMessage(struct WriteByteStream *s, MessageType type, struct NodeIdentity *nodeId)
{
    if (s->writeInt32(type) == -1)
        return -1;

    if (serializeNodeIdentity(s, nodeId) == -1)
        return -1;

    return 0;
}

int deserializeNodeIdentity(struct ReadByteStream *s, struct NodeIdentity *id)
{
    if (s->readInt32((uint32_t*)&id->processId) == -1)
        return -1;

    if (s->readBytes(id->macAddress, 6) == -1)
        return -1;

    return 0;
}

int deserializeMessage(struct ReadByteStream *s,  MessageType *type, struct NodeIdentity *nodeId)
{
    if (s->readInt32((uint32_t*)type) == -1)
        return -1;

    if (deserializeNodeIdentity(s, nodeId) == -1)
        return -1;

    return 0;
}

void SelfNode::recvDgramHandler(struct sockaddr_in* senderAddress, unsigned char *message, size_t messageSize, void *arg)
{
    struct SelfNode *self = (struct SelfNode*)arg;

    struct NodeDescriptor senderNode;
    senderNode.peerAddress = *senderAddress;

    ReadByteStream s;
    s.openStream(message, messageSize);

    MessageType type;
    if (deserializeMessage(&s, &type, &senderNode.id) == -1) {
        logError("Error deserialize message");
        logPosition();
        return;
    }

    if (self->compareWithSelf(&senderNode.id) != 0) {
        switch (type) {
        case ControlResponse: {
            int luminosity;
            int temperature;
            if (s.readInt32((uint32_t*)&luminosity) == -1) {
                logPosition();
                return;
            }
            if (s.readInt32((uint32_t*)&temperature) == -1) {
                logPosition();
                return;
            }
            self->onSensorsInfoReceived(&senderNode, luminosity, temperature);
            break;
        }
        case ControlSet: {
            int brightness;
            if (s.readInt32((uint32_t*)&brightness) == -1) {
                logPosition();
                return;
            }
            self->onDisplayInfoReceived(&senderNode, brightness, (char*)s.buffer, s.bufferSize);
            break;
        }
        default:
            self->onMessageReceived(type, &senderNode);
            break;
        }

    }
}

int SelfNode::run()
{
    logInfo("Running...");
    printNodeIdentity(&this->nodeIdentity);

    this->generateSensorsInfo();

    if (this->becomeWithoutMaster() == -1) {
        logPosition();
        return -1;
    }

    if (this->sensorsEmulationTimer.start() == -1) {
        logPosition();
        return -1;
    }

    logInfo("Run recv loop");
    if (this->net.runRecvLoop(recvDgramHandler, (void*)this) == -1) {
        logPosition();
        return -1;
    }
    logInfo("Exit from recv loop");
    return 0;
}

int SelfNode::becomeWithoutMaster()
{
    logInfo("\tBecome WithoutMaster");
    if (this->state == Master) {
        logInfo("\t\tStop IAmAlive heartbeet timer");
        if (this->iAmAliveHeartbeetTimer.stop()) {
            logPosition();
            return -1;
        }
        logInfo("\t\tStop ControlRequest  timer");
        if (this->controlRequestTimer.stop()) {
            logPosition();
            return -1;
        }
    }

    this->state = WithoutMaster;
    logInfo("\t\tBroadcast WhoIsMaster");
    if (this->broadcastMessage(WhoIsMaster)) {
        logPosition();
        return -1;
    }
    logInfo("\t\tStart WhoIsMaster timer");
    if (this->whoIsMasterTimer.start()) {
        logPosition();
        return -1;
    }
    return 0;
}

int SelfNode::becomeMaster()
{
    logInfo("\tBecome Master");
    this->state = Master;
    logInfo("\t\tBroadcast IAmMaster");
    if (this->broadcastMessage(IAmMaster) == -1) {
        logPosition();
        return -1;
    }
    logInfo("\t\tStart IAmAlive heartbeet timer");
    if (this->iAmAliveHeartbeetTimer.start()) {
        logPosition();
        return -1;
    }

    logInfo("\t\tStart ControlRequest timer");
    if (this->controlRequestTimer.start()) {
        logPosition();
        return -1;
    }
    return 0;
}

int SelfNode::becomeSlave(NodeDescriptor *master)
{
    logInfo("\tBecome Slave");
    if (this->state == Master) {
        logInfo("\t\tStop IAmAlive heartbeet timer");
        if (this->iAmAliveHeartbeetTimer.stop()) {
            logPosition();
            return -1;
        }
        logInfo("\t\tStop ControlRequest  timer");
        if (this->controlRequestTimer.stop()) {
            logPosition();
            return -1;
        }
    }

    this->state = Slave;
    this->myMaster = *master;
    logInfo("\t\tRestart MonitoringMaster timer");
    if (this->monitoringMasterTimer.start() == -1) {
        logPosition();
        return -1;
    }

    return 0;
}

#define MESSAGE_BUFFER_SIZE 8192
unsigned char sendMessageBuffer[MESSAGE_BUFFER_SIZE];

int SelfNode::sendMessage(MessageType type, struct sockaddr_in *peerAddress)
{
    WriteByteStream s;
    s.openStream(sendMessageBuffer, MESSAGE_BUFFER_SIZE);
    if (serializeMessage(&s, type, &this->nodeIdentity) == -1) {
        logError("Error serialize message");
        logPosition();
        return -1;
    }

    size_t size = (size_t)(s.buffer - sendMessageBuffer);

    if (this->net.sendDgram(peerAddress, sendMessageBuffer, size) == -1) {
        logPosition();
        return -1;
    }

    return 0;
}

int SelfNode::broadcastMessage(MessageType type)
{
    WriteByteStream s;
    s.openStream(sendMessageBuffer, MESSAGE_BUFFER_SIZE);
    serializeMessage(&s, type, &this->nodeIdentity);

    size_t size = (size_t)(s.buffer - sendMessageBuffer);

    if (this->net.broadcastDgram(sendMessageBuffer, size) == -1) {
        logPosition();
        return -1;
    }

    return 0;
}

int SelfNode::sendMessageWithSensorInfo(sockaddr_in *peerAddress, int luminosity, int temperature)
{
    WriteByteStream s;
    s.openStream(sendMessageBuffer, MESSAGE_BUFFER_SIZE);
    if (serializeMessage(&s, ControlResponse, &this->nodeIdentity) == -1) {
        logError("Error serialize message");
        logPosition();
        return -1;
    }

    if (s.writeInt32(luminosity) == -1)
        return -1;

    if (s.writeInt32(temperature) == -1)
        return -1;

    size_t size = (size_t)(s.buffer - sendMessageBuffer);

    if (this->net.sendDgram(peerAddress, sendMessageBuffer, size) == -1) {
        logPosition();
        return -1;
    }

    return 0;
}

int SelfNode::sendMessageWithDisplayInfo(int brightness, char *displayText, size_t textLength)
{
    WriteByteStream s;
    s.openStream(sendMessageBuffer, MESSAGE_BUFFER_SIZE);
    if (serializeMessage(&s, ControlSet, &this->nodeIdentity) == -1) {
        logError("Error serialize message");
        logPosition();
        return -1;
    }

    if (s.writeInt32(brightness) == -1)
        return -1;

    if (s.writeBytes((unsigned char*)displayText, textLength) == -1)
        return -1;

    size_t size = (size_t)(s.buffer - sendMessageBuffer);

    if (this->net.broadcastDgram(sendMessageBuffer, size) == -1) {
        logPosition();
        return -1;
    }

    return 0;
}

int SelfNode::compareWithSelf(NodeIdentity *senderId)
{
    return NodeIdentity::compareNodeIdentities(senderId, &this->nodeIdentity);
}

int SelfNode::compareWithCurrentMaster(NodeIdentity *senderId)
{
    return NodeIdentity::compareNodeIdentities(senderId, &this->myMaster.id);
}

void SelfNode::onMessageReceived(MessageType type, struct NodeDescriptor *sender)
{
    struct NodeIdentity *senderId = &sender->id;
    struct sockaddr_in *senderAddress = &sender->peerAddress;

    logInfo("Message received from ");
    printNodeDescriptor(sender);

    switch (type) {
    case WhoIsMaster:
        logInfo("WhoIsMaster received");
        if (this->state == Master) {
            logInfo(" when I am Master");
            int cmp = this->compareWithSelf(senderId);
            if (cmp < 0) {
                logInfo("\t\tBroadcast IAmMaster");
                if (this->broadcastMessage(IAmMaster) == -1) {
                    logPosition();
                    return;
                }
            }
            else if (cmp > 0) {
                this->becomeWithoutMaster();
            }
        }
        else if (this->state == Slave) {
            logInfo(" when I am Slave");
            if (this->compareWithSelf(senderId) < 0) {
                logInfo("\t\tSend PleaseWait");
                if (this->sendMessage(PleaseWait, senderAddress) == -1) {
                    logPosition();
                    return;
                }
            }
        }
        else if (this->state == WithoutMaster) {
            logInfo(" when I am WithoutMaster");
            if (this->compareWithSelf(senderId) < 0) {
                logInfo("\t\tSend PleaseWait");
                if (this->sendMessage(PleaseWait, senderAddress) == -1) {
                    logPosition();
                    return;
                }
            }
        }
        break;
    case IAmMaster:
        logInfo("IAmMaster received");
        if (this->state == WithoutMaster) {
            logInfo(" when I am WithoutMaster");
            int cmp = this->compareWithSelf(senderId);
            if (cmp < 0) {
                logInfo("\t\tSend WhoIsMaster");
                if (this->sendMessage(WhoIsMaster, senderAddress) == -1) {
                    logPosition();
                    return;
                }
            }
            else if (cmp > 0) {
                if (this->becomeSlave(sender) == -1) {
                    logPosition();
                    return;
                }
            }
        }
        else if (this->state == Slave) {
            logInfo(" when I am Slave");
            int cmp = this->compareWithSelf(senderId);
            if (cmp < 0) {
                logInfo("\t\tSend WhoIsMaster");
                if (this->sendMessage(WhoIsMaster, senderAddress) == -1) {
                    logPosition();
                    return;
                }
            }
            else if (cmp > 0) {
                logInfo("\t\tReset master");
                this->becomeSlave(sender);
            }
        }
        else if (this->state == Master) {
            logInfo(" when I am Master");
            int cmp = this->compareWithSelf(senderId);
            if (cmp < 0) {
                logInfo("\t\tBroadcast IAmMaster");
                if (this->broadcastMessage(IAmMaster)) {
                    logPosition();
                    return;
                }
            }
            else if (cmp > 0) {
                this->becomeSlave(sender);
            }
        }
        break;
    case PleaseWait:
        logInfo("PleaseWait received");
        if (this->state == WithoutMaster) {
            logInfo(" when I am WithoutMaster");
            int cmp = this->compareWithSelf(senderId);
            if (cmp > 0) {
                logInfo("\t\tRestart WhoIsMaster timer");
                if (this->waitForMasterTimer.start() == -1) {
                    logPosition();
                    return;
                }
            }
        }
        break;
    case ControlRequest:
        logInfo("ControlRequest received");
        if (this->state == Slave) {
            logInfo(" when I am Slave");
            this->sendMessageWithSensorInfo(&sender->peerAddress, this->luminosity, this->temperature);
        }
        break;
    default:
        break;
    }
}

void SelfNode::onSensorsInfoReceived(NodeDescriptor *sender, int luminosity, int temperature)
{
    if (this->clientsCount == CLIENTS_MAX_COUNT - 1) {
        logPosition();
        return;
    }

    SensorInfo *sensorInfo = &this->receivedSensorsInfo[this->clientsCount];
    sensorInfo->luminosity = luminosity;
    sensorInfo->temperature = temperature;
    ++this->clientsCount;
}

void SelfNode::onDisplayInfoReceived(NodeDescriptor *sender, int brightness, char *displayText, size_t textLength)
{
    logInfo("Display info received\n");
    this->brightness = brightness;
    strncpy(this->displayText, displayText, textLength);
    printf("Brightness: %d, Text: %s\n", this->brightness, this->displayText);
}

void SelfNode::onWhoIsMasterTimeout()
{
    logInfo("WhoIsMaster timeout");
    if (this->becomeMaster() == -1) {
        logPosition();
    }
}

void SelfNode::onMonitoringMasterTimeout()
{
    logInfo("MonitoringMaster timeout");
    this->becomeWithoutMaster();
}

void SelfNode::onWaitForMasterTimeout()
{
    logInfo("WaitForMaster timeout");
    this->becomeWithoutMaster();
}

void SelfNode::onIAmAliveHeartbeetTimeout()
{
    logInfo("MasterAlive timeout");
    if (this->state == Master) {
        logInfo("\t\tBroadcast IAmMaster");
        this->broadcastMessage(IAmMaster);
    }
    else {
        logError("Error: Master Heartbeet timer is not stopped!!");
    }
}

void SelfNode::onControlRequestTimeoutHandler()
{
    logInfo("ControlRequest timeout");

    this->clientsCount = 0;

    if (this->broadcastMessage(ControlRequest) == -1) {
        logPosition();
        return;
    }

    if (this->controlWaitResponceTimer.start() == -1) {
        logPosition();
        return;
    }
}

void SelfNode::onControlWaitResponceTimeoutHandler()
{
    logInfo("ControlWaitResponce timeout");

    if (this->clientsCount == 0) {
        logInfo("Received sensors info is empty");
    }

    int meanTemperature = this->temperature;
    int meanLuminosity = this->luminosity;

    for(int i = 0; i < this->clientsCount; ++i) {
        meanLuminosity += this->receivedSensorsInfo[i].luminosity;
        meanTemperature += this->receivedSensorsInfo[i].temperature;
    }
    meanLuminosity /= this->clientsCount + 1;
    meanTemperature /= this->clientsCount + 1;

    sprintf(this->displayText, "Temperature: %d", temperature);

    int brightness = meanLuminosity * 4 + 1000; // for example

    if (this->sendMessageWithDisplayInfo(brightness, this->displayText, strlen(this->displayText)) == -1) {
        logPosition();
        return;
    }

    this->clientsCount = 0;
}

void SelfNode::onSensorsEmulationTimeout()
{
    logInfo("Sensors values has been changed");
    this->generateSensorsInfo();
}

void SelfNode::generateSensorsInfo()
{
    this->luminosity = rand() % 100 + 1000;
    printf("luminosity = %d\n", this->luminosity);

    this->temperature = rand() % 20 + 10;
    printf("temperature = %d\n", this->temperature);
}

void SelfNode::whoIsMasterTimeoutHandler(TimerHandlerArgument arg)
{
    if (arg.ptrValue)
        ((SelfNode*)arg.ptrValue)->onWhoIsMasterTimeout();
}

void SelfNode::monitoringMasterTimeoutHandler(TimerHandlerArgument arg)
{
    if (arg.ptrValue)
        ((SelfNode*)arg.ptrValue)->onMonitoringMasterTimeout();
}

void SelfNode::waitForMasterTimeoutHandler(TimerHandlerArgument arg)
{
    if (arg.ptrValue)
        ((SelfNode*)arg.ptrValue)->onWaitForMasterTimeout();
}

void SelfNode::iAmAliveHeartbeetTimeoutHandler(TimerHandlerArgument arg)
{
    if (arg.ptrValue)
        ((SelfNode*)arg.ptrValue)->onIAmAliveHeartbeetTimeout();
}

void SelfNode::controlRequestTimeoutHandler(TimerHandlerArgument arg)
{
    if (arg.ptrValue)
        ((SelfNode*)arg.ptrValue)->onControlRequestTimeoutHandler();
}

void SelfNode::controlWaitResponceTimeoutHandler(TimerHandlerArgument arg)
{
    if (arg.ptrValue)
        ((SelfNode*)arg.ptrValue)->onControlWaitResponceTimeoutHandler();
}

void SelfNode::sensorsEmulationTimeoutHandler(TimerHandlerArgument arg)
{
    if (arg.ptrValue)
        ((SelfNode*)arg.ptrValue)->onSensorsEmulationTimeout();
}

int SelfNode::initTimers()
{
    if (Timer::initTimerSystem() == -1) {
        logPosition();
        return -1;
    }

    TimerHandlerArgument arg;
    arg.ptrValue = (void*)this;
    if (this->whoIsMasterTimer.init(5000, false, SelfNode::whoIsMasterTimeoutHandler, arg) == -1) {
        logPosition();
        return -1;
    }
    if (this->monitoringMasterTimer.init(16000, false, SelfNode::monitoringMasterTimeoutHandler, arg) == -1) {
        logPosition();
        return -1;
    }
    if (this->waitForMasterTimer.init(10000, false, SelfNode::waitForMasterTimeoutHandler, arg) == -1) {
        logPosition();
        return -1;
    }
    if (this->iAmAliveHeartbeetTimer.init(10000, true, SelfNode::iAmAliveHeartbeetTimeoutHandler, arg) == -1) {
        logPosition();
        return -1;
    }

    if (this->controlRequestTimer.init(20000, true, SelfNode::controlRequestTimeoutHandler, arg) == -1) {
        logPosition();
        return -1;
    }
    if (this->controlWaitResponceTimer.init(3000, false, SelfNode::controlWaitResponceTimeoutHandler, arg) == -1) {
        logPosition();
        return -1;
    }

    if (this->sensorsEmulationTimer.init(30000, true, SelfNode::sensorsEmulationTimeoutHandler, arg) == -1) {
        logPosition();
        return -1;
    }
    return 0;
}

static void printNodeDescriptor(struct NodeDescriptor *node)
{
    struct NodeIdentity *nodeId = &node->id;
    struct sockaddr_in *senderAddress = &node->peerAddress;
    unsigned char *mac = nodeId->macAddress;

    char ipString[INET_ADDRSTRLEN];
    memset(ipString, 0, INET_ADDRSTRLEN);
    inet_ntop(AF_INET, &senderAddress->sin_addr, ipString, INET_ADDRSTRLEN);
    printf("%s, %d, %02x:%02x:%02x:%02x:%02x:%02x\n", ipString, nodeId->processId,
           mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

static void printNodeIdentity(struct NodeIdentity *nodeId)
{
    unsigned char *mac = nodeId->macAddress;
    printf("%d, %02x:%02x:%02x:%02x:%02x:%02x\n", nodeId->processId,
           mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}
