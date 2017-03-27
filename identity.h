#ifndef IDENTITY_H
#define IDENTITY_H

// close fd, getpid
#include <unistd.h>

struct NodeIdentity
{
    pid_t processId;
    unsigned char macAddress[6];
};

int compareNodeIdentities(struct NodeIdentity *self, struct NodeIdentity *other);

int getSelfNodeIdentity(struct NodeIdentity *id);

#endif // IDENTITY_H
