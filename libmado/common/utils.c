#include <stdint.h>
#include <string.h>
#include <strings.h>

#include "utils.h"

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
