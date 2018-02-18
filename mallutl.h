#include <errno.h> /* for MALLOC_FAILURE_ACTION */
#include <pthread.h>
#include <stdio.h>  /* for printing and debugging */
#include <stddef.h> /* for size_t */
#include <string.h>
#include <sys/mman.h>
#include <unistd.h> /* for sbrk, sysconf */

/* By default errno is ENOMEM */
#ifndef MALLOC_FAILURE_ACTION
#define MALLOC_FAILURE_ACTION errno = ENOMEM;
#endif /* MALLOC_FAILURE_ACTION */

#define MIN_ORDER 5
#define MAX_ORDER 12

/* The memory request must be always multiple of PAGE_SIZE */
#define HEAP_PAGE_SIZE sysconf(_SC_PAGE_SIZE)
#define NUMBER_OF_PROC sysconf(_SC_NPROCESSORS_ONLN)
#define BASE 2
#define SIZE_TO_ORDER(size) (ceil((log(size) / log(BASE))))

#define INIT_PTHREAD_MUTEX(mutex) (memset(mutex, 0, sizeof(pthread_mutex_t)))

typedef struct __block_header_t {
  void *address;
  unsigned order;
  size_t size;
  unsigned is_free;
  unsigned is_mmaped;
  struct __block_header_t *next;
  unsigned __padding;
} block_header_t;

// We don't have fastbin, because we merge the block
// We remove the keepcost since we don't use malloc_trim
// We remove usmblks since this field is maintained only by
// nonthreading environments

struct mallinfo {
  // The total amount of memory allocated not by mmap (<= a page size)
  int arena;
  // Number of ordinary free blocks
  int ordblks;
  // Number of blocks allocated by mmap
  int hblks;
  // Number of bytes allocated by mmap
  int hblkhd;
  // Total size of memory occupied by chunks handed out by malloc
  int uordblks;
  // Total free space
  int fordblks;
  // Total allocation request
  int allocreq;
  // Total free request
  int freereq;
};

typedef struct __arena_header_t {
  pthread_mutex_t arena_lock;
  block_header_t *first_block_address;
  block_header_t *end_block_address;
  size_t size;
  unsigned number_of_heaps;
  unsigned number_of_threads;
  struct __arena_header_t *next;
  struct mallinfo stats;
} arena_header_t;

// Malloc data is used just as fixed address for the start
// of the arena
typedef struct __malloc_data {
  arena_header_t arena;
} malloc_data;

enum free_status {
  occupied = 0,
  empty = 1
};

enum is_mmaped {
  allocated = 0,
  mmaped = 1
};

extern __thread arena_header_t *arena_ptr;
extern malloc_data main_data;

size_t upper_power_of_two(size_t);
void push(block_header_t *);
void remove_node(block_header_t *);
