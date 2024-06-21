#ifndef _STDLIB_H
#define _STDLIB_H 1

#include "sys/cdefs.h"

#ifdef __cplusplus
extern "C" {
#endif

__attribute__((__noreturn__))
void abort(void);
void     *malloc(size_t);				//< The standard function.
void     *realloc(void *, size_t);		//< The standard function.
void     *calloc(size_t, size_t);		//< The standard function.
void      free(void *);					//< The standard function.
int atexit(void (*func) (void));
int atoi(const char*);
char* getenv(const char*);
int abs(int n);
void exit(int);


#ifdef __cplusplus
}
#endif

#endif
