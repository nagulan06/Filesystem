#include <stdint.h>

#include "pages.h"
#include "bitmap.h"


bitmap* bitmap_init()
{
    // The last 32 bytes (256 bits) of block 0 shall be used to store block bitmap
    bitmap* bit_base = (bitmap*)((uint8_t*) pages_get_page(1) - 32);
    return bit_base;
}

void bitmap_set(int n)
{

    bitmap* bit_base = bitmap_init();
    int index = n / 32;
    int pos = n % 32;
    // Set the corresponding bit to indicate that the block is used
    bit_base->bits[index] |= 1 << pos;
}

void bitmap_clear(int n)
{
    bitmap* bit_base = bitmap_init();
    int index = n / 32;
    int pos = n % 32;
    // Clear the corresponding bit to indicate that the block is free
    bit_base->bits[index] &= ~(1 << pos);   
}
int bitmap_get()
{
    bitmap* bit_base = bitmap_init();
    int i;
    // page number 0 is to store inodes and page 1 is for root directory, so start allocating from 2.
    for(i = 2; i<256; i++)
    {
        int val = bit_base->bits[i/32] & (1 << (i%32));
        if(val == 0)
        {
        // i-th bit is not set, hence that block is free
            break;
        }
    }
    // Return the index (block number) that is free to be used.
    bitmap_set(i);
    //bitmap_print();
    return i;
}

void bitmap_print()
{
    bitmap* bit_base = bitmap_init();
    int i;
    printf("Used block numbers: ");
    for(int i=2; i<256; i++)
    {
        int val = bit_base->bits[i/32] & (1 << (i%32));
        if(val)
            printf("%d ", i);
    }
    printf("\n");    
}