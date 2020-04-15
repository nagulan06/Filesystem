
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
#include <time.h>
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

    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    inode* node = get_inode(inum);
    printf("+ storage_stat(%s); inode %d\n", path, inum);
    print_inode(node);

    memset(st, 0, sizeof(struct stat));
    st->st_uid   = getuid();
    st->st_mode  = node->mode;
    st->st_size  = node->size;
    st->st_nlink = 1;
    st->st_atime = ts.tv_sec;
    
    return 0;
}

int read_direct(inode *node, char *buf, size_t size, off_t offset)
{
    size_t new_size = size;
    // Offset in the first page
    if(offset < 4096)
    {
        uint8_t* data = pages_get_page(node->ptrs[0]);
        // If size is greater then a page, read from both the direct pointers
        if(size > 4096)
        {
            printf(" + reading from page: %d; size: %d\n", node->ptrs[0], 4096);
            memcpy(buf, data + offset, 4096);
            data = pages_get_page(node->ptrs[1]);
            new_size = size - 4096;
            printf(" + reading from page: %d; size: %d\n", node->ptrs[1], new_size);
            memcpy(buf+4096, data, size);
        }
        // If size is less then a page, read from only the first block
        else
        {
            printf(" + reading from page: %d; size: %d\n", node->ptrs[0], size);
            memcpy(buf, data + offset, size);
        }
        
    }
    // Read only from the 2nd block
    else
    {
        uint8_t* data = pages_get_page(node->ptrs[1]);
        printf(" + reading from page: %d; size: %d\n", node->ptrs[1], size);
        memcpy(buf, data+offset, size);
    }
    return size;
}

int read_indirect(inode *node, char *buf, size_t size, off_t offset)
{
    if(offset < 2*4096)
    {
        // Start reading from the 
    }
}

int
storage_read(const char* path, char* buf, size_t size, off_t offset)
{
    int inum = tree_lookup(path);
    int fpn_begin, fpn_end, pn, buf_offset;
    off_t new_offset = offset;
    size_t read_size = 0, new_size = size;
    if (inum < 0) {
        return inum;
    }
    inode* node = get_inode(inum);
    printf("+ storage_read(%s); inode %d\n", path, inum);
    print_inode(node);

    if (offset >= node->size) {
        return 0;
    }
    
    if (offset + size >= node->size) {
        size = node->size - offset;
    }

    // Find starting page number
    if(offset < 4096)
        fpn_begin = 0;
    else
    {
        for(int i=1; i<127; i++)
        {
            if(offset < 4096*(i+1) && offset >= 4096*i)
            {
                fpn_begin = i;
                break;
            }
        }
    }

    // Find ending page number
    if(offset+size <= 4096)
        fpn_end = 0;
    else
    {
        for(int i=1; i<127; i++)
        {
            if(offset+size <= 4096*(i+1) && offset+size > 4096*i)
            {
                fpn_end = i;
                break;
            }
        }
    }
    // Read from begining page
    pn = inode_get_pnum(node, fpn_begin);
    uint8_t *data = pages_get_page(pn);
    new_offset = offset - 4096*fpn_begin;
    // Make sure to not read beyond the current page
    if(offset + size > 4096*(fpn_begin+1))
       new_size = 4096*(fpn_begin+1) - offset;
    printf(" + reading from page: %d; size: %ld; buf_offest: %d\n", pn, new_size, 0);
    memcpy(buf, data + new_offset, new_size);
    read_size += new_size;      // Increase the read_size
    buf_offset = read_size;

    // If data is left to read
    if(fpn_begin != fpn_end)
    {
        int i;
        for(i = fpn_begin+1; i<fpn_end; i++)
        {
            pn = inode_get_pnum(node, i);
            uint8_t *data = pages_get_page(pn);
            buf_offset = read_size;
            new_size = 4096;
            printf(" + reading from page: %d; size: %ld; buf_offest: %d\n", pn, new_size, 0);
            memcpy(buf+buf_offset, data, new_size);
            read_size += new_size;   
        }
        
        // Read from the ending block
        pn = inode_get_pnum(node, fpn_end);
        uint8_t *data = pages_get_page(pn);
        buf_offset = read_size;
        new_size = size - read_size;    // Whatever is left to read
        printf(" + reading from page: %d; size: %ld; buf_offest: %d\n", pn, new_size, 0);
        memcpy(buf+buf_offset, data, new_size);
        read_size += new_size;
    }
    return read_size;
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
        printf("+ writing to page: %d, size: %d, @offset: %d\n", node->ptrs[1], new_size, new_offset);
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
    if(new_size)
    {
    // Intialize and use indirect pointer to store data
    if(node->iptr == -1)
    {
        node->iptr = alloc_page();
        indirect_pages* indirect_page = pages_get_page(node->iptr);
        indirect_page->total_count = 0;
    
        int i;
        for(i=0; i<num_blocks-2-1; i++)
        {
            indirect_page->ipages[i] = alloc_page();    // Allocate a new page ; this will be held in the indirect page
            uint8_t* data = pages_get_page(indirect_page->ipages[i]);
            printf("+ writing to page: %d, size: %d, @offset %d\n", indirect_page->ipages[i], 4096, 0);
            indirect_page->total_count += 1;
            // Copy from buffer to data => set data and buffer offset appropriately.
            memcpy(data , buf + buf_offset, 4096);
            //data->sizes[i] = 4096;
            buf_offset += 4096;
            new_size = size - buf_offset;
        }
        indirect_page->ipages[i] = alloc_page();
        uint8_t* data = pages_get_page(indirect_page->ipages[i]);
        printf("+ writing to page: %d, size: %d, @offset: %d \n", indirect_page->ipages[i], new_size, 0);
        indirect_page->total_count += 1;
        memcpy(data , buf + buf_offset, new_size);
        //data->sizes[i] = new_size;
        indirect_page->count = num_blocks - 2;   // Update the number of blocks field
    }
    // Write when indirect pointer already exists
    else
    {
        indirect_pages* indirect_page = pages_get_page(node->iptr);
        int i;
        int count = indirect_page->count;
        // Fill remaining portion of previous block, if any.
        if(offset < (count+2)*4096)
        {
            uint8_t* data = pages_get_page(indirect_page->ipages[count-1]);
            // Make sure to fill only the remaining portion of the page
            if(size > (4096*(count+2)))
                new_size = 4096*(count+2) - offset;
            new_offset = 4096 - new_size;
            printf(" + writing to page: %d, size: %d, @offset %d\n", indirect_page->ipages[count-1], new_size, new_offset);
            indirect_page->total_count += 1;
            memcpy(data + new_offset, buf, new_size);
            new_offset = 0;
            buf_offset = new_size;
            new_size = size - new_size;
        }

        if(new_size)
        {
        for(i=indirect_page->count; i<num_blocks-2-1; i++)
        {
            indirect_page->ipages[i] = alloc_page();
            uint8_t *data = pages_get_page(indirect_page->ipages[i]);
            if(new_offset)
                new_size = 4096*(count+3) - new_offset;
            else
                new_size = 4096;           
            printf("+ writing to page: %d, size = %d, @offset: %d\n", indirect_page->ipages[i], new_size, new_offset);
            indirect_page->total_count += 1;
            memcpy(data + new_offset , buf + buf_offset, new_size);
            //indirect_page->sizes[i] = 4096;
            buf_offset += new_size;
            new_offset = 0;
            new_size = size - buf_offset;
            count++;
        }
        indirect_page->ipages[i] = alloc_page();
        printf("+ writing to page: %d, size: %d, @offset: %d\n", indirect_page->ipages[i], new_size, 0);
        indirect_page->total_count += 1;
        uint8_t *data = pages_get_page(indirect_page->ipages[i]);
        memcpy(data , buf + buf_offset, new_size);
        //data->sizes[i] = new_size;
        indirect_page->count = num_blocks - 2;   // Update the number of blocks field
        }
    }
    }
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
    printf("++++++ file check: mode = %d \n ", mode);
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

slist*
image_list(const char *path)
{
    return nested_list(path);
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
storage_symlink(const char *target, const char *linkpath, int mode)
{
    // Check if the "linkpath" already exists
    if (tree_lookup(linkpath) != -ENOENT) {
        printf("link fail: already exist\n");
        return -EEXIST;
    }
    // Create an new file and make an entry into the parent directory for linkpath
    int rv = storage_mknod(linkpath, mode);
    // Store the target string into the assosciated data block
    int link_inum = tree_lookup(linkpath);
    inode *node = get_inode(link_inum);
    char *name = pages_get_page(node->ptrs[0]);
    strlcpy(name, target, ENT_SIZE);

    printf(" ======= name = %s\n ", name);
    return rv;
}

int 
storage_readlink(const char *pathname, char *buf, size_t bufsiz)
{
    int inum = tree_lookup(pathname);
    if(inum == -ENOENT)
    {
        printf(" -- Error: File does not exist\n");
        return -ENOENT;
    }
    inode *node = get_inode(inum);
    buf = pages_get_page(node->ptrs[0]);
    buf[bufsiz] = 0;
    printf("=========== readlink: %s\n", buf);
    return 0;
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
storage_chmod(const char *path, mode_t mode)
{
    printf ( " =================== mode: %d\n", mode);
    int inum = tree_lookup(path);
    inode *node = get_inode(inum);
    node->mode = mode; 
    return 0;
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
    printf(" ====== SET TIME\n");
    return 0;
}
