//
// Created by Jannik on 09.05.2024.
//
#pragma once

#ifndef NIGHTOS_FCNTL_H
#define NIGHTOS_FCNTL_H
#define O_ACCMODE       00000003
#define O_RDONLY        00000000
#define O_WRONLY        00000001
#define O_RDWR          00000002
#ifndef O_CREAT
#define O_CREAT         00000100
#endif
#ifndef O_PATH
#define O_PATH		    010000000
#endif
#ifndef O_EXCL
#define O_EXCL          00000200
#endif
#ifndef O_TRUNC
#define O_TRUNC         00001000
#endif
#ifndef O_APPEND
#define O_APPEND        00002000
#endif
#ifndef O_NONBLOCK
#define O_NONBLOCK     00004000
#endif
#ifndef O_SYNC
#define O_SYNC         00010000
#endif
#ifndef O_LARGEFILE
#define O_LARGEFILE    00100000
#endif
#ifndef O_DIRECTORY
#define O_DIRECTORY    00200000
#endif
#ifndef O_NOFOLLOW
#define O_NOFOLLOW     00400000        /* don't follow links */
#endif

#define F_DUPFD                0
#define F_GETFD                1
#define F_SETFD                2
#define F_GETFL                3
#define F_SETFL                4


#endif //NIGHTOS_FCNTL_H
