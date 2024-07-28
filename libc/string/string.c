//
// Created by Jannik on 20.04.2024.
//
#include <string.h>
#include <stdarg.h>
#include <limits.h>
#include <stdlib.h>

char *strcpy(char * dest, const char * src) {
    if(dest == NULL || src == NULL) {
        return NULL;
    }

    char *start = dest;
    while (*src) {
        *dest++ = *src++;
    }
    *dest = '\0';
    return start;
}

char *strncpy(char * dest, const char * src, size_t n) {
    if(dest == NULL || src == NULL) {
        return NULL;
    }

    size_t i;

    for (i = 0; i < n && src[i] != '\0'; i++) {
        dest[i] = src[i];
    }

    for ( ; i < n; i++) {
        dest[i] = '\0';
    }

    return dest;
}

char *strcat(char * dest, const char * src) {
    if(dest == NULL || src == NULL) {
        return NULL;
    }

    char* start = dest;

    while (*dest) {
        dest++;
    }

    while (*src) {
        *dest++ = *src++;
    }

    *dest = '\0';

    return start;
}

char *strncat(char * dest, const char * src, size_t n) {
    if(dest == NULL || src == NULL) {
        return NULL;
    }

    char* start = dest;

    while (*dest) {
        dest++;
    }

    while (n-- && *src) {
        *dest++ = *src++;
    }

    *dest = '\0';

    return start;
}

int strcmp(const char * s1, const char * s2) {
    if(s1 == NULL || s2 == NULL) {
        if(s1 == NULL && s2 == NULL) {
            return 0;
        } else if(s2 == NULL) {
            return -1;
        } else {
            return 1;
        }
    }

    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }

    return *(const unsigned char*)s1 - *(const unsigned char*)s2;
}

int strncmp(const char * s1, const char * s2, size_t n) {
    if (!s1 || !s2) return s1 - s2;
    if (n == 0) return 0;

    while (--n && *s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }

    return *(unsigned char *)s1 - *(unsigned char *)s2;
}
char *strchr(const char *s, int c) {
    if (!s) return NULL;

    while (*s != (char)c) {
        if (!*s++) return NULL;
    }

    return (char *)s;
}

size_t strcspn(const char * s, const char * reject) {
    if (!s || !reject) return 0;

    const char *p, *r;
    size_t count = 0;

    for (p = s; *p; ++p) {
        for (r = reject; *r; ++r) {
            if (*p == *r) return count;
        }
        ++count;
    }
    return count;
}

char *strpbrk(const char *s1, const char *s2) {
    s1 += strcspn(s1, s2);

    return *s1 ? (char*) s1 : 0;
}

char *strrchr(const char * s, int c) {
    if (!s) return NULL;

    const char *last = NULL;

    do {
        if (*s == (char)c)
            last = s;
    } while (*s++);

    return (char *)last;
}

size_t strspn(const char *s, const char * accept) {
    if (!s || !accept) return 0;

    const char *p;
    size_t count = 0;

    for (p = s; *p; ++p) {
        const char *a;

        for (a = accept; *a; ++a) {
            if (*p == *a) break;
        }

        if (*a == '\0') return count;

        ++count;
    }

    return count;
}

char *strstr(const char *haystack, const char *needle) {
    if (!haystack || !needle) return NULL;
    if (!*needle) return (char *)haystack;

    for (; *haystack; ++haystack) {
        const char *h, *n;
        for (h = haystack, n = needle; *n && *h && *h == *n; ++h, ++n) {}
        if (!*n) return (char *)haystack;
    }

    return NULL;
}

char *strtok(char *str, const char *delim) {
    static char *last;
    if (!delim) return NULL;
    if (!str) return NULL;

    last = str;

    str = last + strspn(last, delim);
    last = str + strcspn(str, delim);

    if (*last) {
        *last = '\0';
        last++;
    } else {
        last = NULL;
    }

    return *str ? str : NULL;
}

char *strtok_r(char *str, const char *delim, char **saveptr) {
    if (!delim || !saveptr) return NULL;  // Ensure the delimiter and saveptr are not NULL

    if (str == NULL) str = *saveptr;  // If str is NULL, continue where we left off
    if (str == NULL) return NULL;  // If saveptr is also NULL, there's nothing to tokenize

    // Skip leading delimiters to find the start of the next token
    str += strspn(str, delim);
    if (*str == '\0') {  // If we reach the end of the string
        *saveptr = NULL;
        return NULL;
    }

    // Find the end of the token
    char *end = str + strcspn(str, delim);
    if (*end == '\0') {  // If we reach the end of the string
        *saveptr = NULL;
    } else {
        *end = '\0';  // Terminate the token
        *saveptr = end + 1;  // Set saveptr to just after the current token
    }

    return str;  // Return the token
}

// Add this helper function to convert long to string
void long_to_str(long d, char* buf, int base) {
    char *p = buf;
    char *p1, *p2;
    unsigned long ud = d;
    int divisor = 10;

    /* If %d is specified and D is minus, put ‘-’ in the head. */
    if (base == 'd' && d < 0)
    {
        *p++ = '-';
        buf++;
        ud = -d;
    }
    else if (base == 'x')
        divisor = 16;

    /* Divide UD by DIVISOR until UD == 0. */
    do
    {
        int remainder = ud % divisor;

        *p++ = (remainder < 10) ? remainder + '0' : remainder + 'a' - 10;
    }
    while (ud /= divisor);

    /* Terminate BUF. */
    *p = 0;

    /* Reverse BUF. */
    p1 = buf;
    p2 = p - 1;
    while (p1 < p2)
    {
        char tmp = *p1;
        *p1 = *p2;
        *p2 = tmp;
        p1++;
        p2--;
    }
}

int snprintf(char *str, size_t size, const char *format, ...) {
    va_list args;
    va_start(args, format);

    int count = 0;
    const char *ptr = format;
    char *output = str;
    size_t remaining = size;

    while (*ptr != '\0' && remaining > 1) {
        if (*ptr != '%') {
            *output++ = *ptr++;
            count++;
            remaining--;
        } else {
            ptr++; // Skip '%'
            switch (*ptr) {
                case 'd':
                case 'l': {
                    long val = va_arg(args, long);
                    char buf[21]; // Increased buffer size for long
                    long_to_str(val, buf, 10);
                    int len = strlen(buf);
                    if (len < remaining) {
                        for (int i = 0; i < len; i++) {
                            *output++ = buf[i];
                        }
                        count += len;
                        remaining -= len;
                    } else {
                        count += len;
                    }
                    if (*ptr == 'l') ptr++; // Skip 'd' in "ld"
                    break;
                }
                case 's': {
                    char *s = va_arg(args, char*);
                    while (*s != '\0' && remaining > 1) {
                        *output++ = *s++;
                        count++;
                        remaining--;
                    }
                    break;
                }
                    // Add more format specifiers as needed
                default:
                    *output++ = *ptr;
                    count++;
                    remaining--;
                    break;
            }
            ptr++;
        }
    }

    if (size > 0) {
        *output = '\0';
    }

    while (*ptr != '\0') {
        if (*ptr != '%') {
            count++;
        } else {
            ptr++;
            switch (*ptr) {
                case 'd':
                case 'l':
                    va_arg(args, long);
                    count += 21; // Assume max long length
                    if (*ptr == 'l') ptr++;
                    break;
                case 's':
                    count += strlen(va_arg(args, char*));
                    break;
                    // Add more format specifiers as needed
            }
        }
        ptr++;
    }

    va_end(args);
    return count;
}
