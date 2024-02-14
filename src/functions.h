#pragma once

#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include "printf.h"

extern struct block_meta *head;
extern struct block_meta *head_m;

struct block_meta *alloc_lesser(struct block_meta *found, size_t size);
struct block_meta *alloc_greater(struct block_meta *found, size_t size);
void coalesce(void);
void block_meta_init(size_t size);
//static struct block_meta *block_meta_alloc(void);
struct block_meta *find_free_block(size_t size);

void block_meta_add(size_t size);
void block_meta_add_mmap(size_t size);
struct block_meta *request_space(struct block_meta* last, size_t size);

#define BYTE 8
#define MALLOC 0
#define CALLOC 1
#define ALIGN(size) (((size) + (BYTE - 1)) & ~(BYTE - 1))
#define SIZE ALIGN(sizeof(struct block_meta))
#define MMAP_THRESHOLD (128 * 1024)
