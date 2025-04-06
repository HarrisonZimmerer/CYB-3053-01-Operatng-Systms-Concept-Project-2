#define _GNU_SOURCE
#include "alloc.h"

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#define ALIGNMENT 16 /**< The alignment of the memory blocks */

static free_block *HEAD = NULL; /**< Pointer to the first element of the free list */

/**
 * Split a free block into two blocks
 *
 * @param block The block to split
 * @param size The size of the first new split block
 * @return A pointer to the first block or NULL if the block cannot be split
 */
void *split(free_block *block, int size) {
    if((block->size < size + sizeof(free_block))) {
        return NULL;
    }

    void *split_pnt = (char *)block + size + sizeof(free_block);
    free_block *new_block = (free_block *) split_pnt;

    new_block->size = block->size - size - sizeof(free_block);
    new_block->next = block->next;

    block->size = size;

    return block;
}

/**
 * Find the previous neighbor of a block
 *
 * @param block The block to find the previous neighbor of
 * @return A pointer to the previous neighbor or NULL if there is none
 */
free_block *find_prev(free_block *block) {
    free_block *curr = HEAD;
    while(curr != NULL) {
        char *next = (char *)curr + curr->size + sizeof(free_block);
        if(next == (char *)block)
            return curr;
        curr = curr->next;
    }
    return NULL;
}

/**
 * Find the next neighbor of a block
 *
 * @param block The block to find the next neighbor of
 * @return A pointer to the next neighbor or NULL if there is none
 */
free_block *find_next(free_block *block) {
    char *block_end = (char*)block + block->size + sizeof(free_block);
    free_block *curr = HEAD;

    while(curr != NULL) {
        if((char *)curr == block_end)
            return curr;
        curr = curr->next;
    }
    return NULL;
}

/**
 * Remove a block from the free list
 *
 * @param block The block to remove
 */
void remove_free_block(free_block *block) {
    free_block *curr = HEAD;
    if(curr == block) {
        HEAD = block->next;
        return;
    }
    while(curr != NULL) {
        if(curr->next == block) {
            curr->next = block->next;
            return;
        }
        curr = curr->next;
    }
}

/**
 * Coalesce neighboring free blocks
 *
 * @param block The block to coalesce
 * @return A pointer to the first block of the coalesced blocks
 */
void *coalesce(free_block *block) {
    if (block == NULL) {
        return NULL;
    }

    free_block *prev = find_prev(block);
    free_block *next = find_next(block);

    // Coalesce with previous block if it is contiguous.
    if (prev != NULL) {
        char *end_of_prev = (char *)prev + prev->size + sizeof(free_block);
        if (end_of_prev == (char *)block) {
            prev->size += block->size + sizeof(free_block);

            // Ensure prev->next is updated to skip over 'block', only if 'block' is directly next to 'prev'.
            if (prev->next == block) {
                prev->next = block->next;
            }
            block = prev; // Update block to point to the new coalesced block.
        }
    }

    // Coalesce with next block if it is contiguous.
    if (next != NULL) {
        char *end_of_block = (char *)block + block->size + sizeof(free_block);
        if (end_of_block == (char *)next) {
            block->size += next->size + sizeof(free_block);

            // Ensure block->next is updated to skip over 'next'.
            block->next = next->next;
        }
    }

    return block;
}

/**
 * Call sbrk to get memory from the OS
 *
 * @param size The amount of memory to allocate
 * @return A pointer to the allocated memory
 */
//Making And initializing Linked list and adress
void *do_alloc(size_t size) {
    //point to memory allocation
    //printf("4\n");
    void* p = sbrk(0);
    //align
    intptr_t align = (intptr_t)p&(ALIGNMENT-1);
    intptr_t ag = (align == 0)?0:ALIGNMENT - align;

    void *mem = sbrk(size + ag + sizeof(header)); //sbrk returns mem 
    if (mem == (void *)-1) return NULL; // sbrk fails return NULL
    void* head = (void*)((intptr_t)mem + ag);
    header *block = (header*)head;
    block-> size = size;
    block-> magic = 0x01234567;
    return head + sizeof(header);
}

/**
 * Allocates memory for the end user
 *
 * @param size The amount of memory to allocate
 * @return A pointer to the requested block of memory
 */
//adding an dchanging linked list

void *tumalloc(size_t size) {
    size = (size + ALIGNMENT - 1) & ~(ALIGNMENT - 1);
    //printf("1\n");

    free_block *best_fit = NULL;
    free_block *prev = NULL;
    free_block *curr = HEAD;
    free_block *best_fit_prev = NULL;

    //printf("The address of HEAD is: %p\n", (void*)HEAD);

    // Traverse the free list to find the best fit
    while (curr != NULL) {
        if (curr->size >= size) {
            if (!best_fit || curr->size < best_fit->size) {
                best_fit = curr;
                best_fit_prev = prev;
            }
        }
        prev = curr;
        curr = curr->next;
    }

    // If a suitable block is found, allocate from it
    if (best_fit) {
        // Remove best_fit from the free list as you're using it
        if (best_fit_prev) {
            best_fit_prev->next = best_fit->next;
        } else {
            HEAD = best_fit->next;
        }
    
        header *h = (header *)best_fit;
        h->size = size;
        h->magic = 0x01234567;
    
        return (void *)(h + 1);
    }

    // No suitable block found — do a fresh allocation
    //printf("3\n");
    int *ptr = do_alloc(size);
    return ptr;
}


/**
 * Allocates and initializes a list of elements for the end user
 *
 * @param num How many elements to allocate
 * @param size The size of each element
 * @return A pointer to the requested block of initialized memory
 */
void *tucalloc(size_t num, size_t size) {
    size_t total_size = num * size;

    // Optional: check for overflow
    if (size != 0 && total_size / size != num) {
        return NULL;
    }

    void *ptr = tumalloc(total_size);
    if (!ptr) {
        return NULL;
    }

    memset(ptr, 0, total_size);
    return ptr;
}

/**
 * Reallocates a chunk of memory with a bigger size
 *
 * @param ptr A pointer to an already allocated piece of memory
 * @param new_size The new requested size to allocate
 * @return A new pointer containing the contents of ptr, but with the new_size
 */
void *turealloc(void *ptr, size_t new_size) {
    //printf("Error turealloc 1\n");
    void *re = tumalloc(new_size);
    if (re == NULL){
        //printf("Error turealloc 2\n");
        return NULL;
    }
    header *cur = (header *)(ptr - sizeof(header));

    if(cur -> magic == 0x01234567) {
        //printf("Error turealloc 3\n");
        memcpy(re, ptr, cur -> size);
        tufree(ptr);
        return re;
    }

    else {
        //printf("Error with Magic\n");
        tufree(re);
        return NULL;
    }
}

/**
 * Removes used chunk of memory and returns it to the free list
 *
 * @param ptr Pointer to the allocated piece of memory
 */
void tufree(void *ptr) {
    header *cur1 = (header *)ptr - 1;

    if (cur1->magic == 0x01234567) {
        
        free_block *block = (free_block *)cur1;
        block->size = cur1->size;
        block->next = HEAD;
        HEAD = coalesce(block);

    } else {
        return;  
    }
}
