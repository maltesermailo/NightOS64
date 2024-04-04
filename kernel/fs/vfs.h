//
// Created by Jannik on 02.04.2024.
//

#ifndef NIGHTOS_VFS_H
#define NIGHTOS_VFS_H

typedef unsigned int FILE;

FILE* OpenFile(char* path);
FILE* OpenStdIn();
FILE* OpenStdOut();

int write(FILE* file, char* bytes, int len);
int read(FILE* file, char** buffer, int len);

#endif //NIGHTOS_VFS_H
