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
    struct _lnode *forward[1];
} lnode;

typedef struct _skip_list {
    int level;
    lnode *head;
    epdb *epdb;
    long rand_bits;
    long rand_bits_left;
} skip_list;

#define sl_make_node(n) (lnode *)malloc(sizeof(lnode)+((n)*sizeof(lnode *)))
/* static lnode * sl_make_node(int levels) */
/* { */
/*     return (lnode *) malloc(sizeof(lnode) + (levels * sizeof(lnode *))); */
/* } */

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

unsigned int hashdjb2(const char *key)
{
/* 
djb2 a simple hash function
From: http://www.cse.yorku.ca/~oz/hash.html 
*/
    unsigned int hash = 5381;
    unsigned int c;
    while ((c = *key++))
        hash = ((hash << 5) + hash) + c; /* hash * 33 + c */
    return hash;
}

void sl_put(skip_list *list,
            const char *key,
            SEXP s_value)
{
    SEXP pvect;
    int pindex, k;
    lvalue *lval;
    lnode *update[MAX_LEVELS], *p, *q;
    unsigned int hash = hashdjb2(key);

    ep_store(list->epdb, s_value, &pvect, &pindex);    /* FIXME: error check */
    Rprintf("DEBUG: epdb store: %p %d\n", pvect, pindex);
    lval = malloc(sizeof(lvalue));    /* FIXME: error check */
    lval->pvect = pvect;
    lval->index = pindex;
    /* put in list */
    
    p = list->head;
    k = list->level;
    for (k = list->level; k >= 0; k--) {
        while ((q = p->forward[k]) && q->hash_key < hash) p = q;
        update[k] = p;
    }
    if (q->hash_key == hash) {
        /* FIXME: here we will compare the actual key value
           and walk while hash_key is same to try and find a match.
         */
    }
    k = sl_random_level(list);
    if (k > list->level) {
        k = ++list->level;
        update[k] = list->head;
    }
    q = sl_make_node(k);        /* FIXME: error check */
    Rprintf("DEBUG: new sl node level = %d\n", k);
    q->hash_key = hash;
    q->key = key;
    q->value = lval;
    while (k >= 0) {
        p = update[k];
        if (p) {
            q->forward[k] = p->forward[k];
            p->forward[k] = q;
        } else {
            q->forward[k] = NULL;
        }
        k--;
    }
}

int sl_get(skip_list *list,
            const char *key,
            SEXP *s_value)
{
    int hash = hashdjb2(key), found = 0, k;
    lnode *p, *q;

    p = list->head;
    k = list->level;
    while (k >= 0) {
        while ((q = p->forward[k]) && q->hash_key < hash) p = q;
        k--;
    }
    if (q && q->hash_key == hash) {
        /* FIXME: verify match on key */
        found = 1;
        *s_value = VECTOR_ELT(q->value->pvect, q->value->index);
    }
    return found;
}

int sl_remove(skip_list *list, const char *key)
{
    int k, m, found = 0;
    lnode *update[MAX_LEVELS], *p, *q;
    unsigned int hash = hashdjb2(key);

    p = list->head;
    k = m = list->level;
    for (k = list->level; k >= 0; k--) {
        while ((q = p->forward[k]) && q->hash_key < hash) p = q;
        update[k] = p;
    }
    if (q->hash_key == hash) {
        /* FIXME: here we will compare the actual key value
           and walk while hash_key is same to try and find a match.
        */
        found = 1;
        for (k = 0; k <= m; k++) {
            p = update[k];
            if (p->forward[k] != q) break;
            p->forward[k] = q->forward[k];
        }
        ep_remove(list->epdb, q->value->pvect, q->value->index);
        free(q);
        while (list->head->forward[m] == NULL && m > 0) m--;
        list->level = m;
    }
    return found;
}


/* .Call API */

static void _finalize_rdict(SEXP xp)
{
    skip_list *list = (skip_list *)R_ExternalPtrAddr(xp);
    sl_free_list(list);
    R_ClearExternalPtr(xp);
}

SEXP rdict_new()
{
    SEXP xp;

    skip_list *list = sl_make_list();
    xp = R_MakeExternalPtr(list, mkString("rdict skip_list"), R_NilValue);
    PROTECT(xp);
    R_RegisterCFinalizerEx(xp, _finalize_rdict, TRUE);
    UNPROTECT(1);
    return xp;
}

SEXP rdict_put(SEXP xp, SEXP key, SEXP value)
{
    skip_list *list = (skip_list *)R_ExternalPtrAddr(xp);
    sl_put(list, CHAR(STRING_ELT(key, 0)), value);
    return R_NilValue;
}

SEXP rdict_get(SEXP xp, SEXP key)
{
    SEXP ans;
    skip_list *list = (skip_list *)R_ExternalPtrAddr(xp);
    int found = sl_get(list, CHAR(STRING_ELT(key, 0)), &ans);
    if (found)
        return ans;
    else
        return R_NilValue;
}

SEXP rdict_remove(SEXP xp, SEXP key)
{
    skip_list *list = (skip_list *)R_ExternalPtrAddr(xp);
    int found = sl_remove(list, CHAR(STRING_ELT(key, 0)));
    return Rf_ScalarLogical(found);
}
