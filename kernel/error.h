//
// Created by Jannik on 25.10.2024.
//

#ifndef NIGHTOS_ERROR_H
#define NIGHTOS_ERROR_H

#include "../mlibc/abis/linux/errno.h"

int get_last_error();
void set_last_error(int error);
inline void clear_error();

int clear_and_report_error();

#endif // NIGHTOS_ERROR_H
