
#define _GNU_SOURCE
#include <string.h>

#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <assert.h>
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>

#include "directory.h"
#include "pages.h"
#include "slist.h"
#include "util.h"
#include "inode.h"

#define ENT_SIZE 24

void
directory_init()
{
    inode* rn = get_inode(1);   // Update inode structure of root directory
    dirent *dir = pages_get_page(1);
    
    dir->entcount = 0;
    if (rn->mode == 0) {
        rn->size = 0;
        rn->ptrs[0] = 1;    // Root directory always points to page 1.
        rn->mode = 040755;
    }
}

// Search for current directory in the parent directory
int 
lookup(dirent *paren_dir, char *current)
{
    for(int i = 0; i < 256; i++)
    {
       
        if(streq(paren_dir->name, current))
        {   
            return paren_dir->inum;
            break;
        }
        paren_dir++;
    }
    return -ENOENT;
}

// When given a path to a file, this function finds the parent directory and returns its inode number.
int 
find_paren_inode(const char* path)
{
    slist *list = s_split(path+1, '/');
    int inum;
    // If only root directory is present
    if(list->next == NULL)
        return 1;
    dirent *parent_dir = pages_get_page(1);    // Root directory page
    char *parent = '/';
    char *current = list->data;

    while(list && list->next)
    {
        inum = lookup(parent_dir, current);
        list = list->next;
        inode *paren_node = get_inode(inum);
        parent_dir = pages_get_page(paren_node->ptrs[0]);
        current = list->data;
    }
    return inum;
}   

// This functions returns the inode number of a file/directory when given its path
int
tree_lookup(const char* path)
{
    assert(path[0] == '/');

    if (streq(path, "/")) {
        return 1;
    }
    
    slist *list = s_split(path+1, '/');
    int inum;
    dirent *parent_dir = pages_get_page(1);    // Root directory page
    char *parent = "/";
    char *current = list->data;

    while(list)
    {
       // printf(" ===== name in paren dir = %s, current = %s\n", parent_dir->name, current);
        inum = lookup(parent_dir, current);
        inode *paren_node = get_inode(inum);
        parent_dir = pages_get_page(paren_node->ptrs[0]);
        if((list = list->next))
            current = list->data;
    }
   // printf(" ===== inode number returned for path %s is %d\n", path, inum);
    return inum;
}

// Adds a file/directory entry into the parent directory
int
directory_put(const char* path, int inum, int pinum)
{
    dirent *dir;
    int pnum;
  //  printf(" ===== this is the parent dir inode num where file is put: %d\n", pinum);
    if(pinum == 1)
    {
        dir = pages_get_page(1);
        pnum = 1;
    }
    else
    {    
        inode *paren_node = get_inode(pinum);
        dir = pages_get_page(paren_node->ptrs[0]);    // Get the parent directory's page as struct dirent
        pnum = paren_node->ptrs[0];
    }
   // printf(" ==== this is where directory is put (%p), page num = %d \n", dir, pnum);
    const char *name = get_name(path);
    int count = dir->entcount;
    // The first entry always hold the number of entries in the directory
    // Make a new entry if one does not already exist
    if(tree_lookup(path) == -ENOENT)
    {
     //   printf(" ====== directory entry not found");
        dir->entcount += 1;
        dir = dir + count;
     //   printf(" ======= dir written at offset: %p\n", dir);
        // Update the name and inode number of the file to be entered
        strlcpy(dir->name, name, ENT_SIZE);    
        dir->inum = inum;
        //char* ent = pages_get_page(1) + inum*ENT_SIZE;
        printf("+ dirent = '%s'\n", dir->name);
    }
    // If the file to be put in the directory already exist, find the file entry and only update it's inode number.
    else
    {
        for(int i=0; i<count; i++)
        {
            // Find the file
            if(streq(dir->name, name))
            {
                dir->inum = inum;   // Update its inode number to the new inode number
                break;
            }
            dir++;
        }
    }
    
    inode* node = get_inode(inum);
    printf("+ directory_put(..., %s, %d) -> 0\n", dir->name, inum);
    print_inode(node);

    return 0;
}

void 
free_all(inode *del_node, int inum)
{
    // Free all the pages associated with the inode
    // First free the direct pointers
    int pnum = del_node->ptrs[0];
    free_page(pnum);
    if((pnum = del_node->ptrs[1]))
        free_page(pnum);
    // If indirect pages exist for the file, free all of them
    if(del_node->iptr)
    {
        indirect_pages* data = pages_get_page(del_node->iptr);
        int i = 0;
        pnum = data->ipages[i];
        while(pnum)
        {
            i++;
            free_page(pnum);
            pnum = data->ipages[i];
        }
    }
    del_node->size = 0; // Make size 0
    free_inode(inum); // Free its inode
}

// Deletes a file/directory entry
int
directory_delete(const char* path)
{
    const char *name = get_name(path);
    inode *paren_node;
    dirent *dir;
    printf(" + directory_delete(%s)\n", name);

    // Find parent inode value and thus the parent page address
    int pinum = find_paren_inode(path);
    paren_node = get_inode(pinum);
    paren_node->size -= sizeof(dirent);   // Reduce the parent directory size.
    dir = pages_get_page(paren_node->ptrs[0]);

    // Reduce the directory entry count by 1
    int count = dir->entcount;
    dir->entcount -= 1;
    
    int pnum = 0, curr_count = 0;

    // Now find the file to be deleted and free its corresponding inode and pages
    for(int i=0; i<count; i++)
    {
        curr_count++;
        // Find the file
        if(streq(dir->name, name))
        {
            inode *del_node = get_inode(dir->inum);
            // If the node has links, do not free the inode and pages associated with it
            if(del_node->refs == 0)
                free_all(del_node, dir->inum);
            else
                del_node->refs -= 1;            
            break;
        }
        dir++;
    }

    // Now move all the dirent entries up to overwrite current file's entry
    for(int i = curr_count; i < count; i++)
    {
        *dir = *(dir+1);
        dir++;

    }
    return 0;
}


slist* add_files_to_list(dirent *dir)
{
    inode *node;
    slist *list = 0;
    slist *direc = 0;
    // Loop through the directory and find all the files in it and add it to the slist
    dirent *count_dir = pages_get_page(1);
    int count = count_dir->entcount;
    for(int i = 0; i<count; i++)
    {
        list = s_cons(dir->name, list);
        node = get_inode(dir->inum);
        // If the current entry is a directory, add it to another list
        if(node->mode == 040755)
        {
            printf(" ==== %s is a directory\n", dir->name);    
        }
        dir++;
    }
    list = s_rev_free(list);
    return list;
}


slist*
directory_list(const char *path)
{
    int inum, pnum;
    dirent *dir;
    printf("+ directory_list()\n");
  //  printf(" ===== path in list = %s\n", path);
    inum = tree_lookup(path);
  //  printf(" ======= dir inum = %d\n", inum);   
    inode *paren_node = get_inode(inum);
   // printf(" ======= paren inode: %d\n", inum);
   // printf(" ====== current directory from where ls is called: %s ; inum: %d;", path, inum);
   // printf(" ====== Its page using inum: %p ; using node->ptr: %p;", pages_get_page(inum), pages_get_page(paren_node->ptrs[0]));
    if(inum == 1)
    {
        dir = pages_get_page(1);
        pnum = 1;
    }
    else
    {
        dir = pages_get_page(paren_node->ptrs[0]);    // Current directory from where "ls" is called
        pnum = paren_node->ptrs[0];
    }
   // printf(" ====== directory being searched page num = %d\n", pnum);
    slist *ys = add_files_to_list(dir);

    return ys;
}

void
print_directory(inode* dd)
{
    printf("Contents:\n");
    slist* items = directory_list(dd);
    for (slist* xs = items; xs != 0; xs = xs->next) {
        printf("- %s\n", xs->data);
    }
    printf("(end of contents)\n");
    s_free(items);
}

// Returns true if the directory is empty
int 
is_empty(char *path)
{
    int inum = tree_lookup(path);
    inode *node = get_inode(inum);
    if(node->size == 0)
        return 1;
    else
        return 0;
}