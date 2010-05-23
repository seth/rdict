#define MAX_LEVELS 16
#define MAX_LEVEL (MAX_LEVELS - 1)

#include <Rinternals.h>
#include <stdlib.h>
#include "epdb.h"

typedef struct _lnode {
    long hash_key;
    const char *key;
    SEXP key_pvect;
    int key_index;
    SEXP value_pvect;
    int value_index;
    /* forward skip list must come last */
    struct _lnode *forward[1];
} lnode;

typedef struct _skip_list {
    int level;
    lnode *head;
    epdb *epdb;
    long rand_bits;
    int rand_bits_left;
    int item_count;
    int level_stats[1];
} skip_list;

#define sl_make_node(n) (lnode *)malloc(sizeof(lnode)+((n)*sizeof(lnode *)))

skip_list * sl_make_list()
{
    int i;
    skip_list *list = malloc(sizeof(skip_list) + (MAX_LEVELS * sizeof(int)));
    if (!list) return NULL;
    list->head = sl_make_node(MAX_LEVELS);
    if (!list->head) {
        free(list);
        return NULL;
    }
    for (i = 0; i < MAX_LEVELS; i++) {
        list->head->forward[i] = NULL;
        list->level_stats[i] = 0;
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
    list->item_count = 0;
    return list;
}

static void sl_free_lnode(epdb *db, lnode *node)
{
    /* FIXME: should check for errors from ep_remove */
    ep_remove(db, node->key_pvect, node->key_index);
    ep_remove(db, node->value_pvect, node->value_index);
    free(node);
    node = NULL;
}

static void sl_free_list(skip_list *list)
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
    int level = 0, b, i;
    do {
        b = list->rand_bits & 3;
        if (!b) level++;
        list->rand_bits >>= 2;
        for (i = 0; i < 2; i++) {
            if (list->rand_bits_left == 0) {
                list->rand_bits = random();
                list->rand_bits_left = ((sizeof(long) * 8) - 1) / 2;
                break;
            }
            --list->rand_bits_left;
        }
    } while (!b);
    return level > MAX_LEVEL ? MAX_LEVEL : level;
}

unsigned long hashdjb2(const char *key)
{
/* 
djb2 a simple hash function
From: http://www.cse.yorku.ca/~oz/hash.html 
*/
    unsigned long hash = 5381;
    unsigned int c;
    while ((c = *key++))
        hash = ((hash << 5) + hash) + c; /* hash * 33 + c */
    return hash;
}

int sl_put(skip_list *list,
            SEXP key,           /* CHARSXP */
            SEXP s_value)
{
    SEXP key_pvect, value_pvect;
    int key_index, value_index, k, do_replace = 0;
    lnode *update[MAX_LEVELS], *p, *q;
    const char *key_str = CHAR(key);
    unsigned long hash = hashdjb2(key_str);

    if (!ep_store(list->epdb, s_value, &value_pvect, &value_index)
        || !ep_store(list->epdb, key, &key_pvect, &key_index)) {
        return 0;
    }
    /* We don't duplicate s_value, but increment it's named property
       in hopes that other code will duplicate before modification
     */
    SET_NAMED(s_value, 2);

    p = list->head;
    k = list->level;
    for (k = list->level; k >= 0; k--) {
        while ((q = p->forward[k]) && q->hash_key < hash) p = q;
        update[k] = p;
    }
    /* handle hash collisions */
    p = q;
    while (p && p->hash_key == hash) {
        if (p->key == key_str) {
            q = p;
            do_replace = 1;
            break;
        }
        p = p->forward[0];
    }
    
    if (!do_replace) {
        k = sl_random_level(list);
        if (k > list->level) {
            k = ++list->level;
            update[k] = list->head;
        }
        q = sl_make_node(k);        /* FIXME: error check */
        list->level_stats[k]++;
    }

    q->key_pvect = key_pvect;
    q->key_index = key_index;
    q->value_pvect = value_pvect;
    q->value_index = value_index;
    q->hash_key = hash;
    q->key = key_str;
    if (!do_replace) {
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
        list->item_count++;
    }
    return 1;
}

int sl_get(skip_list *list,
            const char *key,
            SEXP *s_value)
{
    unsigned long hash = hashdjb2(key);
    int found = 0, k;
    lnode *p, *q;

    p = list->head;
    k = list->level;
    while (k >= 0) {
        while ((q = p->forward[k]) && q->hash_key < hash) p = q;
        k--;
    }

    p = q;
    while (p && p->hash_key == hash) { /* handle hash collisions */
        if (p->key == key) {
            q = p;
            break;
        }
        p = p->forward[0];
    }
    if (q && q->hash_key == hash) {
        /* FIXME: deal w/ hash collisions */
        found = 1;
        *s_value = VECTOR_ELT(q->value_pvect, q->value_index);
    }
    return found;
}

int sl_remove(skip_list *list, const char *key)
{
    int k, m, found = 0;
    lnode *update[MAX_LEVELS], *p, *q;
    unsigned long hash = hashdjb2(key);

    p = list->head;
    k = m = list->level;
    for (k = list->level; k >= 0; k--) {
        while ((q = p->forward[k]) && q->hash_key < hash) p = q;
        update[k] = p;
    }
    p = q;
    while (p && p->hash_key == hash) { /* handle hash collisions */
        if (p->key == key) {
            q = p;
            break;
        }
        p = p->forward[0];
    }
    if (q && q->hash_key == hash) {
        found = 1;
        for (k = 0; k <= m; k++) {
            p = update[k];
            if (p->forward[k] != q) break;
            p->forward[k] = q->forward[k];
        }
        sl_free_lnode(list->epdb, q);
        while (list->head->forward[m] == NULL && m > 0) m--;
        list->level = m;
        list->item_count--;
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
    int ok = sl_put(list, STRING_ELT(key, 0), value);
    if (!ok) Rf_error("rdict_put failed");
    return R_NilValue;
}

SEXP rdict_mput(SEXP xp, SEXP v)
{
    int i, len = LENGTH(v);
    SEXP keys = Rf_getAttrib(v, R_NamesSymbol);
    skip_list *list = (skip_list *)R_ExternalPtrAddr(xp);
    for (i = 0; i < len; i++) {
        int ok = sl_put(list, STRING_ELT(keys, i),
                        VECTOR_ELT(v, i));
        if (!ok) Rf_error("rdict_put failed at item: %d", i + 1);
    }
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

SEXP Rf_sortVector(SEXP);       /* XXX: undocumented R API */

SEXP rdict_keys(SEXP xp)
{
    SEXP keys;
    skip_list *list = (skip_list *)R_ExternalPtrAddr(xp);
    lnode *q;
    int i;

    PROTECT(keys = Rf_allocVector(STRSXP, list->item_count));
    if (list->item_count > 0) {
        q = list->head->forward[0];
        for (i = 0; i < list->item_count; i++) {
            SET_STRING_ELT(keys, i, VECTOR_ELT(q->key_pvect, q->key_index));
            q = q->forward[0];
        }
        /* XXX: not a documented R API */
        Rf_sortVector(keys);
    }
    UNPROTECT(1);
    return keys;
}

SEXP rdict_stats(SEXP xp)
{
    SEXP ans, ans_nms, levels, keys;
    skip_list *list = (skip_list *)R_ExternalPtrAddr(xp);
    lnode *q;
    int i, *ilev;
    char buf[64];

    /* level, hash_key */
    PROTECT(ans = Rf_allocVector(VECSXP, 2));
    PROTECT(levels = Rf_allocVector(INTSXP, MAX_LEVELS));
    PROTECT(keys = Rf_allocVector(STRSXP, list->item_count));
    SET_VECTOR_ELT(ans, 0, levels);
    SET_VECTOR_ELT(ans, 1, keys);
    PROTECT(ans_nms = Rf_allocVector(STRSXP, 2));
    SET_STRING_ELT(ans_nms, 0, mkChar("levels"));
    SET_STRING_ELT(ans_nms, 1, mkChar("keys"));
    Rf_setAttrib(ans, R_NamesSymbol, ans_nms);

    ilev = INTEGER(levels);
    for (i = 0; i < MAX_LEVELS; i++) {
        ilev[i] = list->level_stats[i];
    }
    
    if (list->item_count > 0) {
        q = list->head->forward[0];
        for (i = 0; i < list->item_count; i++) {
            snprintf(buf, 64, "%lu", q->hash_key);
            SET_STRING_ELT(keys, i, mkChar(buf));
            q = q->forward[0];
        }
    }
    UNPROTECT(4);
    return ans;
}
