#ifndef RDICT_H_
#define RDICT_H_

#include <Rinternals.h>
#include "epdb.h"

struct rdict_value {
    SEXP pvect;
    int index;
};

typedef struct _rdict {
    epdb *epdb;
    /* actual dict details */
} rdict;

rdict * rdict_new(int size_hint);
void rdict_free(rdict *d);

int cput(rdict *d, const char *key, struct rdict_value *value);
int cget(rdict *d, const char *key, struct rdict_value *value);



#endif
