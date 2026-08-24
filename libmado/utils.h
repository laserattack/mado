#ifndef MADO_UTILS_H
#define MADO_UTILS_H

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

#ifdef MADO_UTILS_IMPL

char is_whitespace(char c) {
    return c == ' ' || c == '\t' || c == '\r';
}

int lookup_ident(const char *word,
                 const struct ident_entry *idents,
                 int n_idents) {

    int word_len = (int)strlen(word);
    int best_score = INT32_MIN;
    int best_token = 0;

    for (int i = 0; i < n_idents; i++) {
        int cur_word_len = (int)strlen(idents[i].word);

        // exact match
        if (word_len == cur_word_len &&
            strncasecmp(word, idents[i].word, word_len) == 0) {
            return idents[i].token;
        }

        // Fuzzy match
        int32_t score = fuzzy_match(word, idents[i].word, true);
        if (score > best_score) {
            best_score = score;
            best_token = idents[i].token;
        }
    }

    return best_score != INT32_MIN ? best_token : 0;
}

#endif // MADO_UTILS_IMPL

#endif // MADO_UTILS_H
