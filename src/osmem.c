// SPDX-License-Identifier: BSD-3-Clause

#include "osmem.h"
#include <sys/mman.h>
#include <unistd.h>
#include <string.h>
#include "functions.h"
#include "block_meta.h"


void *allocate_mem(size_t size, int type)
{
	if (size == 0)
		return NULL;

	if ((ALIGN(size) + SIZE > MMAP_THRESHOLD && type == MALLOC) ||
		(ALIGN(size) + SIZE > (unsigned long)getpagesize() && type == CALLOC)) {
		if (!head_m) { // head = NULL - case mmap
			head_m = mmap(NULL, ALIGN(size) + SIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

			if (head_m == MAP_FAILED)
				return NULL;
			head_m->size = ALIGN(size);
			head_m->status = STATUS_MAPPED;
			head_m->next = head_m;
			head_m->prev = head_m;

			return (head_m + 1);
		}
		block_meta_add_mmap(size);
		return (head_m->prev + 1);
	}
	if (!head) { // head = NULL - case sbrk
		block_meta_init(size);
		return (head + 1);
	}
	struct block_meta *block = find_free_block(size);

	if (block == NULL) { // failed to find free block
		if (head->prev->status == STATUS_FREE) {
			sbrk(ALIGN(size) - head->prev->size);
			head->prev->size += ALIGN(size) - head->prev->size;
			head->prev->status = STATUS_ALLOC;
			return (head->prev + 1);
		}
		block_meta_add(size);
		return (head->prev + 1);
	}
	// found free block
	if (ALIGN(size) + SIZE + BYTE <= block->size) {
		size_t old_size = block->size;

		block->size = ALIGN(size);
		block->status = STATUS_ALLOC;

		struct block_meta *new = (struct block_meta *)((char *)block + SIZE + block->size);
		struct block_meta *next = block->next;

		new->prev = block;
		block->next = new;
		new->next = next;
		next->prev = new;
		new->size = old_size - block->size - SIZE;
		new->status = STATUS_FREE;
	} else {
		block->status = STATUS_ALLOC;
	}
	return (block + 1);
}

void *os_malloc(size_t size)
{
	/* TODO: Implement os_malloc */
	if (size == 0)
		return NULL;

	coalesce();

	return allocate_mem(size, MALLOC);
}

void os_free(void *ptr)
{
	/* TODO: Implement os_free */
	if (!ptr)
		return;

	void *ptr2 = (void *)((char *)ptr - SIZE);
	struct block_meta *found = (struct block_meta *)ptr2;

	if (found->status == STATUS_MAPPED) {
		if (found == head_m && found->next == head_m) {
			munmap(found, found->size + SIZE);
			head_m = NULL;
		} else if (found == head_m) {
			found->prev->next = found->next;
			found->next->prev = found->prev;
			head_m = found->next;
			munmap(found, found->size + SIZE);
		} else {
			found->prev->next = found->next;
			found->next->prev = found->prev;
			munmap(found, found->size + SIZE);
		}

	} else if (found->status == STATUS_ALLOC) {
		found->status = STATUS_FREE;
		return;
	}
}

void *os_calloc(size_t nmemb, size_t size)
{
	/* TODO: Implement os_calloc */
	if (nmemb == 0 || size == 0)
		return NULL;

	coalesce();

	size_t size_calloc = nmemb * size;
	void *ptr = (void *)allocate_mem(size_calloc, CALLOC);

	return memset(ptr, 0, size_calloc);
}

void *os_realloc(void *ptr, size_t size)
{
	/* TODO: Implement os_realloc */

	if (ptr == NULL)
		return os_malloc(size);

	if (size == 0) {
		os_free(ptr);
		return NULL;
	}

	coalesce();

	struct block_meta *block_realloc = (struct block_meta *)ptr - 1;

	if (ALIGN(size) + SIZE > MMAP_THRESHOLD) {
		if (block_realloc->status == STATUS_ALLOC) {
			void *mmap_block = os_malloc(size);
			char *dest = (char *)mmap_block;
			char *src = (char *)block_realloc + SIZE;

			for (size_t i = 0; i < block_realloc->size; i++)
				dest[i] = src[i];
			block_realloc->status = STATUS_FREE;
			return (void *)dest;
		}
	}

	if (block_realloc->status == STATUS_FREE)
		return NULL;

	if (block_realloc->size == ALIGN(size))
		return (block_realloc + 1);

	if (block_realloc->status == STATUS_MAPPED) {
		int min_size;

		if (size < block_realloc->size)
			min_size = size;
		else
			min_size = block_realloc->size;

		struct block_meta *new_block = os_malloc(size);

		memcpy(new_block, (block_realloc + 1), min_size);

		os_free(block_realloc + 1);

		return new_block;
	}

	if (block_realloc->status == STATUS_ALLOC) {
		if (ALIGN(size) < block_realloc->size)
			return alloc_lesser(block_realloc, size);

		if (block_realloc->next != head && block_realloc->next->status == STATUS_FREE) {
			size_t new_size = block_realloc->size + block_realloc->next->size + SIZE;

			if (new_size >= ALIGN(size)) {
				if (new_size - ALIGN(size) >= SIZE + BYTE) {
					struct block_meta *new_block = (struct block_meta *)((char *)block_realloc + SIZE + ALIGN(size));

					block_realloc->size = ALIGN(size);
					new_block->next = block_realloc->next->next;
					block_realloc->next = new_block;
					new_block->prev = block_realloc;
					new_block->next->prev = new_block;
					new_block->size = new_size - ALIGN(size) - SIZE;
					new_block->status = STATUS_FREE;
				} else {
					block_realloc->size = new_size;
					block_realloc->next = block_realloc->next->next;
					block_realloc->next->prev = block_realloc;
				}
				return (block_realloc + 1);
			}
		}
		return alloc_greater(block_realloc, size);
	}

	return NULL;
}
