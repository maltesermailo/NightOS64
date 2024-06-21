#ifndef _STDIO_H
#define _STDIO_H 1

#include "sys/cdefs.h"
#include <stddef.h>
#include <stdarg.h>

#define EOF (-1)
#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

#ifdef __cplusplus
extern "C" {
#endif

typedef struct file_object {
    int fd;
} FILE;

extern FILE *stderr;

int fflush(FILE* ptr);
int fprintf(FILE* stream, const char * format, ...);
int fclose(FILE*);
FILE* fopen(const char*, const char*);
size_t fread(void*, size_t, size_t, FILE*);
long int ftell(FILE*);
int fseek(FILE*, long int offset, int origin);
size_t fwrite ( const void * ptr, size_t size, size_t count, FILE * stream );
int sprintf ( char * str, const char * format, ... );
int vfprintf ( FILE * stream, const char * format, va_list arg );
void setbuf ( FILE * stream, char * buffer );

int printf(const char* __restrict, ...);
int putchar(int);
int puts(const char*);


#ifdef __cplusplus
}
#endif

#endif
