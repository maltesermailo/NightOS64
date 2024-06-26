#ifndef _STRING_H
#define _STRING_H 1

#include "sys/cdefs.h"

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

extern int memcmp(const void*, const void*, size_t);
extern void* memcpy(void* __restrict, const void* __restrict, size_t);
extern void* memmove(void*, const void*, size_t);
extern void* memset(void*, int, size_t);
extern void* memchr(const void*, int c, size_t);
extern size_t strlen(const char*);
extern char* strdup(const char*);
extern char *strcpy(char *, const char *);
extern char *strncpy(char *, const char *, size_t);

extern char *strcat(char *, const char *);
extern char *strncat(char *, const char *, size_t);
extern int strcmp(const char *, const char *);
extern int strncmp(const char *, const char *, size_t);
extern char *strchr(const char *, int);
extern size_t strcspn(const char *, const char *);
extern char *strpbrk(const char *, const char *);
extern char *strrchr(const char *, int);
extern size_t strspn(const char *, const char *);
extern char *strstr(const char *, const char *);
extern char *strtok(char *, const char *);
extern char *strtok_r(char *, const char *, char **);

#ifdef __cplusplus
}
#endif

#endif
