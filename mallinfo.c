#include "mallinfo.h"
#include "mallutl.h"

arena_header_t *arena_head = &main_data.arena;
static int i = 0;

struct mallinfo mallinfo() {
  i = 0;
  arena_header_t *arena = arena_head;
  struct mallinfo stats;

  while (arena != NULL) {
    struct mallinfo arena_stats = arena->stats;
    stats.arena += arena_stats.arena;
    stats.ordblks += arena_stats.ordblks;
    stats.hblks += arena_stats.hblks;
    stats.hblkhd += arena_stats.hblkhd;
    stats.uordblks += arena_stats.uordblks;
    stats.fordblks += arena_stats.fordblks;
    stats.allocreq += arena_stats.allocreq;
    stats.freereq += arena_stats.freereq;

    i++;
    arena = arena->next;
  }

  return stats;
}

void malloc_stats() {
  struct mallinfo total_stats = mallinfo();

  char buf[1024];
  // Print header
  snprintf(buf, 1024, "---------------------\n");
  write(STDOUT_FILENO, buf, strlen(buf) + 1);

  snprintf(buf, 1024, "Total arena allocated: %ul\n", total_stats.arena);
  write(STDOUT_FILENO, buf, strlen(buf) + 1);

  snprintf(buf, 1024, "Number of arena: %ld\n", i);
  write(STDOUT_FILENO, buf, strlen(buf) + 1);

  int index = 0;
  arena_header_t *arena = arena_head;

  while (arena != NULL) {
    struct mallinfo arena_stats = arena->stats;

    // Print title
    snprintf(buf, 1024, "---------------------\n");
    write(STDOUT_FILENO, buf, strlen(buf) + 1);

    snprintf(buf, 1024, "Arena: %d\n", index);
    write(STDOUT_FILENO, buf, strlen(buf) + 1);

    // Print arena
    snprintf(buf, 1024, "Space allocated (bytes): %d\n", arena_stats.arena);
    write(STDOUT_FILENO, buf, strlen(buf) + 1);

    // Print number of free blocks
    snprintf(buf, 1024, "Free blocks: %d\n", arena_stats.ordblks);
    write(STDOUT_FILENO, buf, strlen(buf) + 1);

    // Print mmaped region
    snprintf(buf, 1024, "Mmaped region: %d\n", arena_stats.hblks);
    write(STDOUT_FILENO, buf, strlen(buf) + 1);

    // Print mmaped space
    snprintf(buf, 1024, "Mmaped region (bytes): %d\n", arena_stats.hblkhd);
    write(STDOUT_FILENO, buf, strlen(buf) + 1);

    // Print occupied space
    snprintf(buf, 1024, "Occupied space: %d\n", arena_stats.uordblks);
    write(STDOUT_FILENO, buf, strlen(buf) + 1);

    // Print free space
    snprintf(buf, 1024, "Free space: %d\n", arena_stats.fordblks);
    write(STDOUT_FILENO, buf, strlen(buf) + 1);

    // Print alloc request
    snprintf(buf, 1024, "Malloc request: %d\n", arena_stats.allocreq);
    write(STDOUT_FILENO, buf, strlen(buf) + 1);

    // Print free request
    snprintf(buf, 1024, "Free request: %d\n", arena_stats.freereq);
    write(STDOUT_FILENO, buf, strlen(buf) + 1);

    index++;
    arena = arena->next;
  }
}
