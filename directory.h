#ifndef DIRECTORY_H
#define DIRECTORY_H

#include <bsd/string.h>

#include "slist.h"
#include "pages.h"
#include "inode.h"

#define DIR_NAME 24

typedef struct dirent {
    char name[DIR_NAME];
    int  inum;
    int entcount;
} dirent;

char* directory_get(int inum);
void directory_init();
int directory_lookup(const char* name);
int tree_lookup(const char* path);
int directory_put(const char* name, int inum, int pinum);
int directory_delete(const char* name);
int find_paren_inode(const char* path);
slist* directory_list(const char *path);
void print_directory();
int is_empty(char *path);
slist* nested_list(const char *path);

#endif

