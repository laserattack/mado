#ifndef MADO_UTILS_H
#define MADO_UTILS_H

#include <string.h>
#include <strings.h>

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
    int matches_count = 0, matched_token = 0;

    for (int i = 0; i < n_idents; i++) {
        int cur_word_len = (int)strlen(idents[i].word);
        if (word_len <= cur_word_len &&
            strncasecmp(word, idents[i].word, word_len) == 0) {
            matched_token = idents[i].token;
            if (word_len == cur_word_len)
                return matched_token;
            matches_count++;
        }
    }

    return (matches_count == 1) ? matched_token : 0;
}

#endif // MADO_UTILS_IMPL

#endif // MADO_UTILS_H
