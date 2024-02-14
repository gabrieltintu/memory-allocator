// SPDX-License-Identifier: BSD-3-Clause

#include "functions.h"
#include <sys/mman.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>
#include "osmem.h"
#include "block_meta.h"

struct block_meta *head;
struct block_meta *head_m;

struct block_meta *alloc_greater(struct block_meta *block, size_t size)
{
	size_t new_size = ALIGN(size);

	if (block->next == head) {
		sbrk(new_size - block->size);
		block->size = new_size;
		return (block + 1);
	}

	if (block->size < new_size) {
		struct block_meta *new_block = (struct block_meta *)os_malloc(size);

		block->status = STATUS_FREE;

		int min_size;

		if (size < block->size)
			min_size = size;
		else
			min_size = block->size;

		char *dest = (char *)new_block;
		char *src = (char *)block + SIZE;

		for (int i = 0; i < min_size; i++)
			dest[i] = src[i];

		return (void *)dest;
	}
	if (block->size - new_size >= SIZE + BYTE) {
		size_t old_size = block->size;

		block->size = new_size;
		block->status = STATUS_ALLOC;

		struct block_meta *new = (struct block_meta *)((char *)block + SIZE + block->size);
		struct block_meta *next = block->next;

		new->prev = block;
		block->next = new;
		new->next = next;
		next->prev = new;
		new->size = old_size - block->size;
		new->status = STATUS_FREE;
	}
		return (block + 1);
}

struct block_meta *alloc_lesser(struct block_meta *block, size_t size)
{
	if (block->size - ALIGN(size) >= SIZE + BYTE) {
		size_t new_size = block->size;

		block->size = ALIGN(size);
		block->status = STATUS_ALLOC;

		struct block_meta *new = (struct block_meta *)((char *)block + SIZE + block->size);
		struct block_meta *next = block->next;

		new->prev = block;
		block->next = new;
		new->next = next;
		next->prev = new;
		new->size = new_size - block->size - SIZE;
		new->status = STATUS_FREE;
	}

	return (block + 1);
}

void coalesce(void)
{
	struct block_meta *current = head;

	if (head == NULL)
		return;

	if (current->next == head)
		return;

	while (current->next != head) {
		while (current->next != head && current->status == STATUS_FREE && current->next->status == STATUS_FREE) {
			current->size += current->next->size + SIZE;
			current->next = current->next->next;
			current->next->prev = current;
		}
		current = current->next;
	}
}

struct block_meta *find_free_block(size_t size)
{
	struct block_meta *current = head;
	struct block_meta *found = NULL;
	struct block_meta *found_best = NULL;
	size_t min_size;

	if (current->status == STATUS_FREE && current->size >= ALIGN(size)) {
		found = current;
		found_best = current;
			min_size = current->size;
	} else {
		min_size = 999999;
	}

	current = current->next;

	while (current != head) {
		if (current->status == STATUS_FREE && current->size >= ALIGN(size)) {
			found = current;
			if (current->size < min_size) {
				min_size = current->size;
				found_best = current;
			}
		}
		current = current->next;
	}

	if (found_best == NULL)
		return found;
	else
		return found_best;
}

void block_meta_init(size_t size)
{
	head = sbrk(MMAP_THRESHOLD);
	head->size = MMAP_THRESHOLD - SIZE;
	head->status = STATUS_ALLOC;
	head->prev = head;
	head->next = head;

	if (ALIGN(size) + SIZE + SIZE > MMAP_THRESHOLD)
		return;

	head->size = ALIGN(size);
	struct block_meta *block = (struct block_meta *)((char *)head + SIZE + head->size);

	block->size = MMAP_THRESHOLD - head->size - SIZE - SIZE;
	block->status = STATUS_FREE;
	block->prev = head;
	block->next = head;
	head->next = block;
	head->prev = block;
}

void block_meta_add_mmap(size_t size)
{
	struct block_meta *last = head_m->prev;
	struct block_meta *new_last;

	new_last = mmap(NULL, ALIGN(size) + SIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

	DIE(new_last == NULL, "Cannot add to list");

	new_last->size = ALIGN(size);
	new_last->status = STATUS_MAPPED;

	new_last->next = head_m;
	new_last->prev = last;
	last->next = new_last;
	head_m->prev = new_last;
}

void block_meta_add(size_t size)
{
	struct block_meta *last = head->prev;
	struct block_meta *new_last;

	new_last = sbrk(SIZE + ALIGN(size));

	DIE(new_last == NULL, "Cannot add to list");

	new_last->size = ALIGN(size);
	new_last->status = STATUS_ALLOC;

	new_last->next = head;
	new_last->prev = last;
	last->next = new_last;
	head->prev = new_last;
}
