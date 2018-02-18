/*------------FREE---------------*/
#include "mallutl.h" /* for data structure */
#include "free.h"
#include <math.h>

void merge_if_possible(block_header_t *);
int disallocate_memory(void *);

void free(void *ptr) {
  // Return directly if address is invalid
  if (ptr == NULL)
    return;

  // Disallocate memory
  int result;
  pthread_mutex_lock(&arena_ptr->arena_lock);
  arena_ptr->stats.freereq += 1;
  if ((result = disallocate_memory(ptr)) == -1)
    return;
  pthread_mutex_unlock(&arena_ptr->arena_lock);
}

int disallocate_memory(void *ptr) {
  block_header_t *block = ptr - sizeof(block_header_t);

  // Check header information, whether it is an mmap or not
  // Munmap
  int result;
  if (block->is_mmaped == mmaped) {
    size_t temp = block->size;
    if ((result = munmap((char *)block - block->__padding, block->size)) ==
        -1) {
      return -1;
    }
    arena_ptr->stats.hblks -= 1;
    arena_ptr->stats.hblkhd -= temp;
  } else {
    // Change the status to free for request under 4096
    block->is_free = empty;

    // Update stats
    arena_ptr->stats.uordblks -= block->size;
    arena_ptr->stats.fordblks += block->size;
    arena_ptr->stats.ordblks += 1;

    merge_if_possible(block);
  }
  return 0;
}

void merge_if_possible(block_header_t *header) {
  if (header->next == NULL) {
    return;
  }
  block_header_t *current = header;

  while (1) {
    if (current == NULL || current->next == NULL)
      break;
    if (current->size == current->next->size) {
      if (current->is_free && current->next->is_free) {
        current->size += 1;
        block_header_t *temp = current->next;
        current->next = temp->next;
        temp = NULL;
        current = header;
        continue;
      }
      current = current->next->next;
    } else {
      current = current->next;
    }
  }
}
