#ifndef IDENTITY_H
#define IDENTITY_H

// close fd, getpid
#include <unistd.h>

struct NodeIdentity
{
    pid_t processId;
    unsigned char macAddress[6];
public:
    static int compareNodeIdentities(struct NodeIdentity *self, struct NodeIdentity *other);
    static int getSelfNodeIdentity(struct NodeIdentity *id);
};

#endif // IDENTITY_H
