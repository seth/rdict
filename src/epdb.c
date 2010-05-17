#include "epdb.h"


static int _free_list_push(epdb *db, SEXP pv, int i)
{
    struct pnode *new_node = malloc(sizeof(struct pnode));
    if (!new_node) return 0;
    new_node->next = db->free_list;
    db->free_list = new_node;
    return 1;
}

static void _free_list_pop(epdb *db, SEXP *pv, int *i)
{
    struct pnode *node = db->free_list;
    *pv = node->pvect;
    *i = node->index;
    db->free_list = node->next;
    free(node);
    node = NULL;
}

static void _free_list_free(epdb *db)
{
    struct pnode *node = db->free_list;
    struct pnode *tmp_node;
    while (node) {
        tmp_node = node->next;
        free(node);
        node = tmp_node;
    }
    db->free_list = NULL;
}

static struct pvect * _make_pvect_node(int v_size, SEXP pv)
{
    int did_preserve = 0;
    struct pvect *pnode;

    if (!pv) {
        pv = Rf_allocVector(VECSXP, v_size);
        R_PreserveObject(pv);
        did_preserve = 1;
    }
    pnode = malloc(sizeof(struct pvect));
    if (!pnode) {
        if (did_preserve) R_ReleaseObject(pv);
        return NULL;
    }
    pnode->pvect = pv;
    pnode->free_index = 0;
    pnode->next = NULL;
    return pnode;
}

epdb * ep_new(int v_size)
{
    epdb *db;
    struct pvect *pvnode;
    SEXP pv;

    /* do this prior to other allocations so that failure doesn't
       cause a leak.
     */
    pv = Rf_allocVector(VECSXP, v_size);
    R_PreserveObject(pv);

    db = malloc(sizeof(epdb));
    if (!db) return NULL;

    pvnode = _make_pvect_node(v_size, pv);
    if (!pvnode) {
        free(db); db = NULL;
        R_ReleaseObject(pv);
        return NULL;
    }
    
    db->free_list = NULL;
    db->pvect_list = pvnode;
    db->pcount = 0;
    db->v_size = v_size;
    return db;
}

void ep_free(epdb *db)
{
    struct pvect *pvnode = db->pvect_list, *pvnode_tmp;
    _free_list_free(db);
    
    while (pvnode) {
        pvnode_tmp = pvnode->next;
        if (pvnode->pvect) R_ReleaseObject(pvnode->pvect);
        free(pvnode);
        pvnode = pvnode_tmp;
    }
    free(db);
    db = NULL;
}

int ep_store(epdb *db, SEXP s, SEXP *pv, int *i)
{
    
    if (db->free_list) {
        _free_list_pop(db, pv, i);
    } else {
        struct pvect *pvnode = db->pvect_list;
        *pv = pvnode->pvect;
        if (pvnode->free_index < LENGTH(*pv)) {
            *i = pvnode->free_index;
            pvnode->free_index++;
        } else {                /* need a new pvect */
            pvnode = _make_pvect_node(db->v_size, NULL);
            if (!pvnode) return 0;
            *pv = pvnode->pvect;
            *i = 0;
            pvnode->next = db->pvect_list;
            db->pvect_list = pvnode;
        }
    }
    db->pcount++;
    SET_VECTOR_ELT(*pv, *i, s);
    return 1;
}

int ep_remove(epdb *db, SEXP pv, int i)
{
    int status;
    status = _free_list_push(db, pv, i);
    SET_VECTOR_ELT(pv, i, R_NilValue);
    return status;
    /* XXX: if ep_remove is misused, we could have the pvect's
       free_index coninciding with something on the free_list.  Could
       have ep_store verify that the spot is NULL, or have remove do
       some checking, but would require finding the pv.
     */
}
