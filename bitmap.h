#ifndef BITMAP_H
#define BITMAP_H

typedef struct bitmap
{
    int bits[8];
}bitmap;

bitmap* bitmap_init();
void bitmap_set(int n);
int bitmap_get();
void bitmap_clear(int n);
void bitmap_print();

#endif