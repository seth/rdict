#define MAX_LEVELS 16
#define MAX_LEVEL (MAX_LEVELS - 1)

#include <Rinternals.h>
#include <stdlib.h>
#include "epdb.h"

typedef struct _lvalue {
    SEXP pvect;
    int index;
} lvalue;

typedef struct _lnode {
    int hash_key;
    const char *key;
    lvalue *value;
    struct _lnode **forward;
} lnode;

typedef struct _skip_list {
    int level;
    lnode *head;
    epdb *epdb;
    long rand_bits;
    long rand_bits_left;
} skip_list;

lnode * sl_make_node(int levels)
{
    return (lnode *) malloc(sizeof(lnode) + (levels * sizeof(lnode *)));
}

skip_list * sl_make_list()
{
    int i;
    skip_list *list = malloc(sizeof(skip_list));
    if (!list) return NULL;
    list->head = sl_make_node(MAX_LEVELS);
    if (!list->head) {
        free(list);
        return NULL;
    }
    for (i = 0; i < MAX_LEVELS; i++) {
        list->head->forward[i] = NULL;
    }
    list->epdb = ep_new(1024);  /* FIXME hard-coded v_size */
    if (!list->epdb) {
        free(list->head);
        free(list);
        return NULL;
    }
    list->level = 0;
    list->rand_bits = random();
    list->rand_bits_left = ((sizeof(long) * 8) - 1) / 2;
    return list;
}

void sl_free_list(skip_list *list)
{
    lnode *s, *t;
    if (list) {
        s = list->head;
        while (s) {
            t = s->forward[0];
            free(s);
            s = t;
        }
        ep_free(list->epdb);
        free(list);
        list = NULL;
    }
}

int sl_random_level(skip_list *list)
{
    int level = 0, b;
    do {
        b = list->rand_bits & 3;
        if (!b) level++;
        list->rand_bits >>= 2;
        if (--(list->rand_bits_left) == 0) {
            list->rand_bits = random();
            list->rand_bits_left = ((sizeof(long) * 8) - 1) / 2;
        }
    } while (!b);
    return level > MAX_LEVEL ? MAX_LEVEL : level;
}

void sl_put(skip_list *list,
            const char *key,
            SEXP s_value)
{
    /* compute hash code */
    /* store s_value in epdb and create an lvalue */
    /* put in list */
}
