#ifndef PAGES_H
#define PAGES_H

#include <stdio.h>

// Indirect pages is meant to hold the list of pages to which the files are mapped to
// This page being 4k, can hold 1024 integers. But we have only 128 inodes (so max. 128 page numbers to hold). So a lot of space is free here
// Hence, some stuff like 'count values' that can help implement read and write easily are stored here for reference
typedef struct indirect_pages
{
    int ipages[256];    // The pages to which the file is mapped to
    int count;          // The number of pages to which the file is mapped to
    int total_count;;
}indirect_pages;

void pages_init(const char* path, int create);
void pages_free();
void* pages_get_page(int pnum);
int alloc_page();
void free_page(int pnum);

#endif
