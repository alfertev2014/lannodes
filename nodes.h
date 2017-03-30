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

    ControlRequest,
    ControlResponse,
    ControlSet
};


struct NodeDescriptor
{
    struct sockaddr_in peerAddress;
    struct NodeIdentity id;
};

struct SensorInfo
{
    int temperature;
    int luminosity;
};

#define DISPLAY_TEXT_MAX_SIZE 1024
#define CLIENTS_MAX_COUNT 128

struct SelfNode
{
private:
    struct NodeIdentity nodeIdentity;

    enum NodeState state;

    struct NodeDescriptor myMaster;
    bool masterIsAvailable;

    struct Networking net;

    char displayText[DISPLAY_TEXT_MAX_SIZE];
    int brightness;

    int temperature;
    int luminosity;

    SensorInfo receivedSensorsInfo[CLIENTS_MAX_COUNT];
    int clientsCount;

    struct Timer whoIsMasterTimer,
            waitForMasterTimer,
            monitoringMasterTimer,
            iAmAliveHeartbeetTimer,

            controlRequestTimer,
            controlWaitResponceTimer,

            sensorsEmulationTimer;

public:
    int init(struct NetworkingConfig *netConfig);
    int run();

private:
    int becomeWithoutMaster();
    int becomeMaster();
    int becomeSlave(struct NodeDescriptor *master);

    int sendMessage(enum MessageType type, sockaddr_in *peerAddress);
    int broadcastMessage(enum MessageType type);

    int sendMessageWithSensorInfo(sockaddr_in *peerAddress, int luminosity, int temperature);

    int sendMessageWithDisplayInfo(int brightness, char *displayText, size_t textLength);

    int compareWithSelf(struct NodeIdentity *senderId);
    int compareWithCurrentMaster(struct NodeIdentity *senderId);

    static void recvDgramHandler(struct sockaddr_in* senderAddress, unsigned char *message, size_t messageSize, void *arg);

    void onMessageReceived(enum MessageType type, struct NodeDescriptor *sender);

    void onSensorsInfoReceived(struct NodeDescriptor *sender, int luminosity, int temperature);
    void onDisplayInfoReceived(struct NodeDescriptor *sender, int brightness, char *displayText, size_t textLength);

    int initTimers();


    static void whoIsMasterTimeoutHandler(TimerHandlerArgument arg);
    static void monitoringMasterTimeoutHandler(TimerHandlerArgument arg);
    static void waitForMasterTimeoutHandler(TimerHandlerArgument arg);
    static void iAmAliveHeartbeetTimeoutHandler(TimerHandlerArgument arg);

    static void controlRequestTimeoutHandler(TimerHandlerArgument arg);
    static void controlWaitResponceTimeoutHandler(TimerHandlerArgument arg);

    static void sensorsEmulationTimeoutHandler(TimerHandlerArgument arg);


    void onWhoIsMasterTimeout();
    void onMonitoringMasterTimeout();
    void onWaitForMasterTimeout();
    void onIAmAliveHeartbeetTimeout();

    void onControlRequestTimeoutHandler();
    void onControlWaitResponceTimeoutHandler();

    void onSensorsEmulationTimeout();

    void generateSensorsInfo();
    void displayInfo();
};

#endif // NODES_H
