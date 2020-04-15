
#include <stdint.h>

#include "pages.h"
#include "inode.h"
#include "util.h"

const int INODE_COUNT = 256;

inode*
get_inode(int inum)
{
    uint8_t* base = (uint8_t*) pages_get_page(0);
    inode* nodes = (inode*)(base);
    return &(nodes[inum]);
}

int
alloc_inode()
{
    for (int ii = 1; ii < INODE_COUNT; ++ii) {
        inode* node = get_inode(ii);
        if (node->mode == 0) {
            memset(node, 0, sizeof(inode));
            node->mode = 010644;
            printf("+ alloc_inode() -> %d\n", ii);
            return ii;
        }
    }

    return -1;
}

void
free_inode(int inum)
{
    printf("+ free_inode(%d)\n", inum);

    inode* node = get_inode(inum);
    memset(node, 0, sizeof(inode));
}

void
print_inode(inode* node)
{
    if (node) {
        printf("node{mode: %04o, size: %d}\n",
               node->mode, node->size);
    }
    else {
        printf("node{null}\n");
    }
}

// This function returns the exact page number of the page number index is given
int inode_get_pnum(inode* node, int fpn)
{
    if(fpn == 0)
        return node->ptrs[0];
    if(fpn == 1)
        return node->ptrs[1];
    // If fpn is greater than 1, it has to be fetched from the indirect page.
    if(fpn > 1)
    {
        indirect_pages *ipage = pages_get_page(node->iptr);
        return ipage->ipages[fpn-2];
    }
}
