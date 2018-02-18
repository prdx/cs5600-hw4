/* Author: Anak Agung Ngurah Bagus Trihatmaja
 Malloc library using buddy allocation algorithm
 S01E02
*/

#include "mallutl.h" /* for data structure */
#include "malloc.h"
#include <math.h>

block_header_t *find_suitable_space(size_t);
void *request_memory(size_t);
void fill_header(block_header_t *, size_t);
int is_need_split(block_header_t *, size_t);
void split(block_header_t *, size_t);
void *init_malloc(size_t, const void *);
int init_arena();
arena_header_t *find_arena();
void push_arena(arena_header_t *);

__thread arena_header_t *arena_ptr;

// Check if malloc has been called previously
static unsigned has_malloc_initiated = 0;

malloc_data main_data = { 0 };
static int number_of_arenas = 1;
static int number_of_requests = 0;
static pthread_mutex_t malloc_thread_init_lock = PTHREAD_MUTEX_INITIALIZER;

typedef void *(*__hook)(size_t __size, const void *);
__hook __malloc_hook = (__hook)init_malloc;

void lock_all() {
  pthread_mutex_lock(&malloc_thread_init_lock);
  arena_header_t* arena = &main_data.arena;

  while (arena != NULL) {
    pthread_mutex_lock(&arena->arena_lock);
    arena = arena->next;
  }
}

void unlock_all() {
  arena_header_t* arena = &main_data.arena;

  while (arena != NULL) {
    pthread_mutex_unlock(&arena->arena_lock);
    arena = arena->next;
  }

  pthread_mutex_unlock(&malloc_thread_init_lock);
}

void pthread_atfork_prepare(void) {
  lock_all();
}

void pthread_atfork_parent(void) {
  unlock_all();
}

void pthread_atfork_child(void) {
  unlock_all();
}

/*------------INIT MALLOC----------*/
// Define it to look like malloc
void *init_malloc(size_t size, const void *caller) {
  if (pthread_atfork(pthread_atfork_prepare, pthread_atfork_parent, pthread_atfork_child))
    return NULL;

  // Init the first arena
  if (init_arena())
    return NULL;

  __malloc_hook = NULL;
  // call malloc
  return malloc(size);
}

int init_arena() {
  // If malloc has been done at the beginning, no need to init arena
  if (has_malloc_initiated)
    return 0;

  arena_ptr = &main_data.arena;
  INIT_PTHREAD_MUTEX(&arena_ptr->arena_lock);
  arena_ptr->number_of_heaps = 1;
  arena_ptr->number_of_threads = 1;
  arena_ptr->next = NULL;
  arena_ptr->stats.arena = 0;
  arena_ptr->stats.ordblks = 0;
  arena_ptr->stats.hblks = 0;
  arena_ptr->stats.hblkhd = 0;
  arena_ptr->stats.uordblks = 0;
  arena_ptr->stats.allocreq = 0;
  arena_ptr->stats.freereq = 0;

  has_malloc_initiated = 1;
  return 0;
}

int init_thread_arena() {
  pthread_mutex_lock(&malloc_thread_init_lock);

  // Update number of request
  number_of_requests++;

  // If the arena is full
  if (number_of_arenas == NUMBER_OF_PROC) {
    arena_ptr = find_arena();
    arena_ptr->number_of_threads++;
    pthread_mutex_unlock(&malloc_thread_init_lock);
    return 0;
  }

  arena_header_t *arena = NULL;

  size_t size = sizeof(arena_header_t);
  if ((arena = (arena_header_t *)sbrk(size)) == NULL) {
    MALLOC_FAILURE_ACTION;
    pthread_mutex_unlock(&malloc_thread_init_lock);
    return 1;
  }

  INIT_PTHREAD_MUTEX(&arena->arena_lock);
  arena->number_of_threads = 1;
  arena->number_of_heaps = 1;
  arena->stats.arena = 0;
  arena->stats.ordblks = 0;
  arena->stats.hblks = 0;
  arena->stats.hblkhd = 0;
  arena->stats.uordblks = 0;
  arena->stats.allocreq = 0;
  arena->stats.freereq = 0;
  arena->next = NULL;

  push_arena(arena);

  // Update arena pointer
  arena_ptr = arena;

  // Update number of arenas
  number_of_arenas++;

  pthread_mutex_unlock(&malloc_thread_init_lock);
  return 0;
}

void push_arena(arena_header_t *new_arena) {
  arena_header_t *arena = &main_data.arena;

  // Find the tail of our arena
  while (arena->next != NULL) {
    arena = arena->next;
  }

  arena->next = new_arena;
}

arena_header_t *find_arena() {
  arena_header_t *arena = &main_data.arena;
  int number_of_loops = number_of_requests % NUMBER_OF_PROC;
  int i = 0;

  for (i; i < number_of_loops; i++)
    arena = arena->next;

  return arena;
}

/*------------MALLOC---------------*/
void *malloc(size_t size) {

  // Malloc hook
  __hook lib_hook = __malloc_hook;
  if (lib_hook != NULL)
    return (*lib_hook)(size, __builtin_return_address(0));

  // Check if arena_ptr exists
  if (arena_ptr == NULL && init_thread_arena()) {
    MALLOC_FAILURE_ACTION;
    return NULL;
  }

  // If it does not exist create a new arena
  pthread_mutex_lock(&arena_ptr->arena_lock);

  // Update stats
  arena_ptr->stats.allocreq += 1;

  // If request is 0, we return NULL
  if (size == 0)
    return NULL;
  // If request is smaller than 8, we round it to 8
  if (size < 8)
    size = 8;

  // Round request to the nearest power of 2
  size_t total_size = sizeof(block_header_t) + size;
  total_size = upper_power_of_two(total_size);

  // Skip this part if there is empty space for request below 4096
  block_header_t *empty_block;
  void *block;

  if (total_size > HEAP_PAGE_SIZE ||
      (empty_block = find_suitable_space(total_size)) == NULL) {

    // Request memory to the OS
    if ((block = request_memory(total_size)) == NULL) {
      MALLOC_FAILURE_ACTION;
      pthread_mutex_unlock(&arena_ptr->arena_lock);
      return NULL;
    }

    fill_header(block, total_size);
    if (total_size <= HEAP_PAGE_SIZE) {
      empty_block = block;
      empty_block->order = MAX_ORDER;
      empty_block->size = HEAP_PAGE_SIZE;
    }
    // Link the new added node to the list
    // FIXME: Skip link if using mmap, because it will cause bug
    if (total_size <= HEAP_PAGE_SIZE) {
      push(block);
    }
  }

  if (total_size <= HEAP_PAGE_SIZE && empty_block != NULL) {
    // Check if split is necessary, if yes, perform split
    block = empty_block;
    if (is_need_split(block, size) == 1) {
      // Split according buddy allocation
      split(block, total_size);
    }
  }

  // Change the status
  block_header_t *temp = block;
  temp->is_free = occupied;

  // Update stats
  if (size <= HEAP_PAGE_SIZE) {
    arena_ptr->stats.uordblks += temp->size; 
  }

  pthread_mutex_unlock(&arena_ptr->arena_lock);

  // Return the address of the data section
  return (char *)block + sizeof(block_header_t);
}

// Request memory to the OS
void *request_memory(size_t size) {
  void *block;

  // Allocate the memory
  if (size <= HEAP_PAGE_SIZE) {
    size = HEAP_PAGE_SIZE;
  }

  if ((block = mmap(NULL, size, PROT_READ | PROT_WRITE,
                    MAP_ANONYMOUS | MAP_PRIVATE, -1, 0)) == (void *)-1) {
    MALLOC_FAILURE_ACTION;
    return NULL;
  }

  // Update stats
  if (size <= HEAP_PAGE_SIZE) {
    arena_ptr->stats.arena += HEAP_PAGE_SIZE; 
  } 
  else {
    arena_ptr->stats.hblks += 1;
    arena_ptr->stats.hblkhd += size;
  }

  return block;
}

void fill_header(block_header_t *block, size_t size) {
  block->address = (char *)block + sizeof(block_header_t);
  block->is_free = empty;
  block->order = SIZE_TO_ORDER(size);
  block->is_mmaped = size > HEAP_PAGE_SIZE ? mmaped : allocated;
  block->next = NULL;
  block->__padding = 0;
  block->size = size;
}

block_header_t *find_suitable_space(size_t size) {
  block_header_t *temp = arena_ptr->first_block_address;
  while (temp != NULL) {
    // DEBUG
    // char buf[1024];
    // snprintf(buf, 1024, "In find suitable space, at: %p \n", temp);
    // write(STDOUT_FILENO, buf, strlen(buf) + 1);

    if (temp->is_free == empty && temp->size >= size)
      return temp;
    temp = temp->next;
  }
  return NULL;
}

int is_need_split(block_header_t *block, size_t size) {
  if (block->size / 2 < size)
    return 0;
  return 1;
}

void split(block_header_t *block, size_t size) {
  if (block->size / 2 <= size && block->size >= size)
    return;

  // Fill all the data
  block->order -= 1;
  block->size /= 2;
  void *temp = (char *)block + block->size;
  block_header_t *buddy = (block_header_t *)temp;
  fill_header(buddy, block->size);
  buddy->is_mmaped = allocated;
  buddy->is_free = empty;
  buddy->next = block->next;
  block->next = buddy;

  // Update stats
  arena_ptr->stats.ordblks += 1;

  // Recursively split again if the block still too large
  split(block, size);
}

