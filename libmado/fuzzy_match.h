#ifndef FUZZY_MATCH_H
#define FUZZY_MATCH_H

#include <stdbool.h>
#include <stdint.h>

int32_t fuzzy_match(const char *pattern, const char *str, bool ignore_case);

#endif /* FUZZY_MATCH_H */
