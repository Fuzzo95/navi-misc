/* -*- mode: c; c-basic-offset: 4; -*- */

#include <config.h>
#include <glib.h>
#include <stdio.h>
#include <string.h>
#include "pagestorage.h"
#include "bptree.h"


gint compare_int_func(gconstpointer a, gconstpointer b, gpointer user_data)
{
    return (*(const int*)a) - (*(const int*)b);
}

int main(int argc, char** argv)
{
    RtgPageStorage* storage;
    RtgBPTree* tree;
    int key;
    gdouble value;

    storage = rtg_page_storage_mapped_new("foo.rtg", 0);

    tree = rtg_bptree_new(storage, "My Tree", sizeof(key), sizeof(value),
			  compare_int_func, NULL);


    rtg_bptree_close(tree);
    rtg_page_storage_close(storage);
    return 0;
}

/* The End */
