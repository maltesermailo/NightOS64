//
// Created by Jannik on 20.04.2024.
//
#include <string.h>

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
    if(dest == NULL || src == NULL) {
        if(dest == NULL && src == NULL) {
            return 0;
        } else if(src == NULL) {
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