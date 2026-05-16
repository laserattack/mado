#ifndef DA_H
#define DA_H

#include <stdio.h>
#include <stdlib.h>

#define DA_INIT_CAP 1

typedef struct {
    int cap;
    int len;
} DaHeader;

#define daheader(arr) ((arr)? (DaHeader *)((void *)(arr) - sizeof(DaHeader)): NULL)
#define dalen(arr)    ((arr)? (daheader(arr))->len: 0)
#define dacap(arr)    ((arr)? (daheader(arr))->cap: 0)

#define dafree(arr)                                                        \
    do {                                                                   \
        if (!(arr)) break;                                                 \
        free(daheader(arr));                                               \
        (arr) = NULL;                                                      \
    } while(0)

#define daclear(arr)                                                       \
    do {                                                                   \
        if (!(arr)) break;                                                 \
        (daheader(arr))->len = 0;                                          \
    } while(0)

#define dapush(arr, el)                                                    \
    do {                                                                   \
        DaHeader *h_;                                                      \
        if (!(arr)) {                                                      \
            h_ = malloc(sizeof(DaHeader) + DA_INIT_CAP * sizeof(*(arr)));  \
            if (!h_) {                                                     \
                fprintf(stderr, "da alloc err\n");                         \
                exit(1);                                                   \
            }                                                              \
            h_->cap = DA_INIT_CAP;                                         \
            h_->len = 0;                                                   \
            (arr) = (void *)h_ + sizeof(DaHeader);                         \
        }                                                                  \
        h_ = daheader(arr);                                                \
        h_->len++;                                                         \
        if (h_->len > h_->cap) {                                           \
            h_->cap *= 2;                                                  \
            h_ = realloc(h_, sizeof(DaHeader) + h_->cap * sizeof(*(arr))); \
            if (!h_) {                                                     \
                fprintf(stderr, "da realloc err\n");                       \
                exit(1);                                                   \
            }                                                              \
            (arr) = (void *)h_ + sizeof(DaHeader);                         \
        }                                                                  \
        (arr)[h_->len-1] = el;                                             \
    } while(0)

#define dainsert(arr, idx, el)                                             \
    do {                                                                   \
        if (!(arr)) break;                                                 \
        int idx_ = (idx);                                                  \
        if (idx_ <= dalen(arr)) {                                          \
            dapush(arr, el);                                               \
            for (int i_ = dalen(arr) - 1; i_ > idx_; i_--)                 \
                (arr)[i_] = (arr)[i_ - 1];                                 \
            (arr)[idx_] = el;                                              \
        }                                                                  \
    } while(0)

#define daremove(arr, idx)                                                 \
    do {                                                                   \
        if (!(arr)) break;                                                 \
        int idx_ = (idx);                                                  \
        if (idx_ < dalen(arr)) {                                           \
            for (int i_ = idx_; i_ < dalen(arr) - 1; i_++)                 \
                (arr)[i_] = (arr)[i_ + 1];                                 \
            daheader(arr)->len--;                                          \
        }                                                                  \
    } while(0)

#define dashrink(arr)                                                      \
    do {                                                                   \
        if (!(arr)) break;                                                 \
        DaHeader *h_;                                                      \
        h_ = daheader(arr);                                                \
        h_->cap = h_->len;                                                 \
        if (!h_->cap) {                                                    \
            dafree(arr);                                                   \
        } else {                                                           \
            h_ = realloc(h_, sizeof(DaHeader) + h_->cap * sizeof(*(arr))); \
            if (!h_) {                                                     \
                fprintf(stderr, "da realloc err\n");                       \
                exit(1);                                                   \
            }                                                              \
            (arr) = (void *)h_ + sizeof(DaHeader);                         \
        }                                                                  \
    } while(0)

#endif
