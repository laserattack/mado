#ifndef UTIL_H
#define UTIL_H

void die(const char *errstr, ...);
char *trim(char *str);

#ifdef UTIL_IMPL

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void die(const char *errstr, ...) {
    va_list ap;
    va_start(ap, errstr);
    vfprintf(stderr, errstr, ap);
    va_end(ap);
    fprintf(stderr, "\n");
    exit(1);
}

char *trim(char *str) {
    if (!str)
        return str;

    while (isspace((unsigned char)*str))
        str++;

    if (*str == 0)
        return str;

    char *end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end))
        end--;
    end[1] = '\0';

    return str;
}

#endif // UTIL_IMPL

#endif // UTIL_H
