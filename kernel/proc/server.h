//
// Created by Jannik on 02.08.2024.
//

#ifndef NIGHTOS_SERVER_H
#define NIGHTOS_SERVER_H

enum srv_op {
    SRV_SEND = 1,
    SRV_RECEIVE,
    SRV_CALL,
    SRV_REGISTER,
    SRV_UNREGISTER,
    SRV_AUTH //this operation is specifically for the server to authenticate requests against the security framework
};

#define SRV_FLAG_NOBLOCKING 1<<0

int srvctl(enum srv_op operation, int flags, void* message);

#endif //NIGHTOS_SERVER_H
