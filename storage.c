
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <assert.h>
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <alloca.h>
#include <string.h>
#include <libgen.h>
#include <bsd/string.h>
#include <stdint.h>

#include "storage.h"
#include "slist.h"
#include "util.h"
#include "pages.h"
#include "inode.h"
#include "directory.h"

#define ENT_SIZE 24
void
storage_init(const char* path, int create)
{
    //printf("storage_init(%s, %d);\n", path, create);
    pages_init(path, create);
    if (create) {
        directory_init();
    }
}

int
storage_stat(const char* path, struct stat* st)
{
    printf("+ storage_stat(%s)\n", path);
    int inum = tree_lookup(path);
    if (inum < 0) {
        printf(" ======= returning inum of %d\n", inum);
        return inum;
    }

    inode* node = get_inode(inum);
    printf("+ storage_stat(%s); inode %d\n", path, inum);
    print_inode(node);

    memset(st, 0, sizeof(struct stat));
    st->st_uid   = getuid();
    st->st_mode  = node->mode;
    st->st_size  = node->size;
    st->st_nlink = 1;
    return 0;
}

int
write_direct_pointers(inode* node, const char* buf, size_t size, off_t offset)
{
    if(node->ptrs[1] == -1)
        node->ptrs[1] = alloc_page();
    size_t new_size = size;
    off_t new_offset = offset;
    off_t buf_offset = 0;
    // Fill remaining portion of the first block and then fill the next block
    if(offset < 4096)
    {
        uint8_t* data = pages_get_page(node->ptrs[0]);   
        // Make sure to fill only the available portion of the first block
        if(size > (4096 - offset))
            new_size = 4096 - offset;
        printf("+ writing to page: %d, size: %d, @offset: %d\n", node->ptrs[0], new_size, offset);
        memcpy(data + offset, buf, new_size);   
        new_offset = 0;
        buf_offset = new_size;
        new_size = size - new_size;
    }
    // Fill the second block
    if(new_size)
    {
        if(new_offset != 0)
            new_offset -= 4096;
        uint8_t* data = pages_get_page(node->ptrs[1]);
        printf("+ writing to page: %d, size: %d, @offset: %d\n", node->ptrs[1], size, new_offset);
        memcpy(data + new_offset, buf+buf_offset, new_size);  
    }
    return size;
}

int 
write_indirect_pointer(inode* node, const char* buf, size_t size, off_t offset)
{
    int num_blocks = (node->size-1)/4096 + 1;
    size_t new_size = size;
    off_t new_offset = offset;
    off_t buf_offset = 0;

    // Fill remaining portion of the previous block if any
    if(offset < 2*4096)
    {
        uint8_t* data = pages_get_page(node->ptrs[1]);   
        new_offset -= 4096;
        // Make sure to fill only the available portion of the first block
        if(size > (4096*2 - offset))
            new_size = 4096*2 - offset;
        printf("+ writing to page: %d, size: %d, @offset %d\n", node->ptrs[1], new_size, 0);
        memcpy(data + new_offset, buf, new_size);   
        new_offset = 0;
        buf_offset = new_size;
        new_size = size - new_size;
    }
    // Intialize and use indirect pointer to store data
    if(node->iptr == -1)
    {
        node->iptr = alloc_page();
        indirect_pages* data = pages_get_page(node->iptr);
    
        int i;
        for(i=0; i<num_blocks-2-1; i++)
        {
            data->ipages[i] = alloc_page();
            printf("+ writing to page: %d, size: %d, @offset %d\n", data->ipages[i], 4096, 0);
            // Copy from buffer to data => set data and buffer offset appropriately.
            memcpy(data , buf + buf_offset, 4096);
            data->sizes[i] = 4096;
            buf_offset += 4096;
            new_size = size - buf_offset;
        }
        data->ipages[i] = alloc_page();
        printf("+ writing to page: %d, size: %d, @offset: %d \n", data->ipages[i], new_size, 0);
        memcpy(data , buf + buf_offset, new_size);
        data->sizes[i] = new_size;
        data->count = num_blocks - 2;   // Update the number of blocks field
    }
    // Write when indirect pointer already exists
    else
    {
        indirect_pages* data = pages_get_page(node->iptr);
        int i;
        printf(" ==== verify count: %d\n", data->count);
        for(i=data->count; i<num_blocks-2-1; i++)
        {
            data->ipages[i] = alloc_page();
            printf("+ writing to page: %d, size = %d, @offset: %d\n", data->ipages[i], 4096, 0);
            memcpy(data , buf + buf_offset, 4096);
            data->sizes[i] = 4096;
            buf_offset += 4096;
            new_size = size - buf_offset;
        }
        data->ipages[i] = alloc_page();
        printf("+ writing to page: %d, size: %d, @offset: %d\n", data->ipages[i], new_size, 0);
        memcpy(data , buf + buf_offset, new_size);
        data->sizes[i] = new_size;
        data->count = num_blocks - 2;   // Update the number of blocks field
    }
    return size;   
}

int
storage_read(const char* path, char* buf, size_t size, off_t offset)
{
    int inum = tree_lookup(path);
    if (inum < 0) {
        return inum;
    }
    inode* node = get_inode(inum);
    printf("+ storage_read(%s); inode %d\n", path, inum);
    print_inode(node);

    if (offset >= node->size) {
        return 0;
    }
    printf(" ===== reading: offset: %d, size: %d, node->size = %d\n", offset, size, node->size);
    if (offset + size >= node->size) {
        size = node->size - offset;
         printf("size changed to %d\n", size);
    }

    uint8_t* data = pages_get_page(inum);
    printf(" + reading from page: %d\n", inum);
    memcpy(buf, data + offset, size);

    return size;
}

int
storage_write(const char* path, const char* buf, size_t size, off_t offset)
{
    int trv = storage_truncate(path, offset + size);
    if (trv < 0) {
        return trv;
    }

    int inum = tree_lookup(path);
    if (inum < 0) {
        return inum;
    }

    inode* node = get_inode(inum);

    // If the file size is less than a page
    if(node->size <= 4096)
    {
        uint8_t* data = pages_get_page(node->ptrs[0]);   
        printf("+ writing to page: %d\n", node->ptrs[0]);
        printf(" ===== File size = %d\n", node->size);
        memcpy(data + offset, buf, size);
        return size;
    }
    // If the file size is going to exceed a page but not 2 pages (no need of indirect pointers)
    if(node->size > 4096 && node->size <= 8192)
    {
        return(write_direct_pointers(node, buf, size, offset));
    }
    // If the file is too large, make use of indirect pointers
    else
    {
        return(write_indirect_pointer(node, buf, size, offset));
    }
  
/*
    uint8_t* data = pages_get_page(inum);
    printf("+ writing to page: %d\n", inum);
    memcpy(data + offset, buf, size);
    return size;
*/
    return 0;
}

int
storage_truncate(const char *path, off_t size)
{
    int inum = tree_lookup(path);
    if (inum < 0) {
        return inum;
    }

    inode* node = get_inode(inum);
    node->size = size;
    return 0;
}

int
storage_mknod(const char* path, int mode)
{
    //printf("++++++ file check: %d  Dir check: %d\n", S_ISREG(mode), S_ISDIR(mode));
    //char* tmp1 = alloca(strlen(path));
    //char* tmp2 = alloca(strlen(path));
    //strcpy(tmp1, path);
    //strcpy(tmp2, path);

    if (tree_lookup(path) != -ENOENT) {
        printf("mknod fail: already exist\n");
        return -EEXIST;
    }

    int    inum = alloc_inode();
    inode* node = get_inode(inum);
    node->mode = mode;
    node->size = 0;
    // Find a free block and update the direct pointer in inode to point to that block.
    node->ptrs[0] = alloc_page();
    node->ptrs[1] = -1;
    node->iptr = -1;
    node->refs = 0;
   
    // Find the parent inode number (which will be a directory) and increase it's size
    int pinum = find_paren_inode(path);
    inode *paren_node = get_inode(pinum);
    paren_node->size += sizeof(dirent);

    printf("+ mknod create %s [%04o] - #%d\n", path, mode, inum);
    //const char* name = get_name(path);
    
    return directory_put(path, inum, pinum);
}

slist*
storage_list(const char* path)
{   
    return directory_list(path);
}

int
storage_unlink(const char* path, int mode)
{
    // Check if such a file exists
    if(tree_lookup(path) == -ENOENT)
    {
        printf("--- error: No such file or directory '%s'\n", get_name(path));
        return -ENOENT;
    }
    // If the entry to be deleted is a dierctory (indicated through mode), check if it is empty
    if(mode)
    {
        if(is_empty(path))
        {
            return directory_delete(path);
        }
        else
        {
            printf("+ Directory '%s' is not empty\n", get_name(path));
            return -ENOTEMPTY;
        }
        
    }
    else
        return directory_delete(path);
}

int
storage_link(const char* from, const char* to)
{
    // Check if the "from" file exist
    int inum = tree_lookup(from);
    if(inum == -ENOENT)
    {
        printf(" --Error: No such file or directory\n");
        return -ENOENT;
    }
    // Check if the "to" file exists
    if (tree_lookup(to) != -ENOENT) {
        printf("link fail: already exist\n");
        return -EEXIST;
    }
    // Find the parent directory of the file "from" => this is the directory where the to file is to be plcaed
    // The reference count for the node is incremented.
    int pinum = find_paren_inode(from);
    inode *node = get_inode(inum);
    node->refs += 1;
    
    // Directory put is called with the inode number of "from" file.
    return directory_put(to, inum, pinum);
}
        

int
storage_rename(const char* from, const char* to)
{
    int inum = tree_lookup(from);
    if (inum == -ENOENT) {
        printf("+ error: rename failed: '%s' no such file or direcory\n", get_name(from));
        return inum;
    }
    char *name_from = get_name(from);
    char *name_to = get_name(to);
    int pinum = find_paren_inode(from);
    inode *paren_node = get_inode(pinum);
    dirent* dir = pages_get_page(paren_node->ptrs[0]);    // Get the parent directory's page as struct dirent
    int count = dir->entcount;

    for(int i=0; i<count; i++)
    {
        // Find the file
        if(streq(dir->name, name_from))
        {
            strlcpy(dir->name, name_to, ENT_SIZE);   // Update its name to the new name
            break;
        }
        dir++;
    }

    return 0;
}

int
storage_set_time(const char* path, const struct timespec ts[2])
{
    // Maybe we need space in a pnode for timestamps.
    return 0;
}
