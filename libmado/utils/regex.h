#ifndef REGEX_H
#define REGEX_H

#include <regex.h>

int regex_match(const char *name, regex_t *regex);
char *regex_extract_first_group(const char *line, regex_t *regex);

#ifdef REGEX_IMPL

#include <stdlib.h>
#include <string.h>

int regex_match(const char *name, regex_t *regex) {
    return regexec(regex, name, 0, NULL, 0) == 0;
}

char *regex_extract_first_group(const char *line, regex_t *regex) {
    regmatch_t matches[2];

    if (regexec(regex, line, 2, matches, 0) == 0 && matches[1].rm_so != -1) {
        int len = matches[1].rm_eo - matches[1].rm_so;
        char *result = (char *)malloc(len + 1);
        if (result) {
            strncpy(result, line + matches[1].rm_so, len);
            result[len] = '\0';
        }
        return result;
    }
    return NULL;
}

#endif // REGEX_IMPL

#endif // REGEX_H
