#ifndef EPDB_H_
#define EPDB_H_

#include <R.h>
#include <Rinternals.h>

struct pnode {
    SEXP pvect;
    int index;
    struct pnode *next;
};

struct pvect {
    SEXP pvect;
    int free_index;
    struct pvect *next;
};

typedef struct _epdb {
    struct pnode *free_list;
    struct pvect *pvect_list;
    int pcount;
    int v_size;
} epdb;

/* Create a new external protection db

   v_size - size used for each protecting VECSXP

   When a protecting vector fills, another of size v_size will be
   allocated.
 */
epdb * ep_new(int v_size);

/* Release an external protection db */
void ep_free(epdb *db);

/* Add a SEXP to the epdb so that it will be protected

   s - the SEXP to protect
   
   pv - (output parameter) the protecting VECSXP in which s was
   placed.

   i - (output parameter) the index in pv in which s was stored.

   Clients need to keep track of the pv and i return values to be able
   to remove s from the store.
 */
int ep_store(epdb *db, SEXP s, SEXP *pv, int *i);

/* Remove the SEXP in protection vector pv and index i from the
   protecting db
 */
int ep_remove(epdb *db, SEXP pv, int i);


#endif
