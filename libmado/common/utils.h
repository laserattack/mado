#ifndef UTILS_H
#define UTILS_H

#include <stdint.h>
#include <string.h>
#include <strings.h>

#include "fuzzy_match.h"

struct ident_entry {
    const char *word;
    int token;
};

char is_whitespace(char c);
int lookup_ident(const char *word,
                 const struct ident_entry *idents,
                 int n_idents);

#endif // UTILS_H
