#ifndef MADO_UTILS_H
#define MADO_UTILS_H

#include <string.h>
#include <strings.h>

struct keyword_entry {
    const char *word;
    int token;
};

char is_whitespace(char c);
int lookup_keyword(const char *word,
                   const struct keyword_entry *keywords,
                   int n_keywords);

#ifdef MADO_UTILS_IMPL

char is_whitespace(char c) {
    return c == ' ' || c == '\t' || c == '\r';
}

int lookup_keyword(const char *word,
                   const struct keyword_entry *keywords,
                   int n_keywords) {
    int word_len = (int)strlen(word);
    int matches_count = 0, matched_token = 0;

    for (int i = 0; i < n_keywords; i++) {
        int cur_word_len = (int)strlen(keywords[i].word);
        if (word_len <= cur_word_len &&
            strncasecmp(word, keywords[i].word, word_len) == 0) {
            matched_token = keywords[i].token;
            if (word_len == cur_word_len)
                return matched_token;
            matches_count++;
        }
    }

    return (matches_count == 1) ? matched_token : 0;
}

#endif // MADO_UTILS_IMPL

#endif // MADO_UTILS_H
