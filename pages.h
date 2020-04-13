#ifndef PAGES_H
#define PAGES_H

#include <stdio.h>

typedef struct indirect_pages
{
    int ipages[256];    // The pages to which the file is mapped to
    int sizes[256];     // Size stored in each page
    int count;          // The number of pages to which the file is mapped to
}indirect_pages;

void pages_init(const char* path, int create);
void pages_free();
void* pages_get_page(int pnum);
int alloc_page();
void free_page(int pnum);

#endif
