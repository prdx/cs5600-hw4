# DESIGN OVERVIEW

## Per-core malloc arenas

The number of arenas created depends on how many cores in the CPU. In CCIS machine, it has 32 cores, so there will be 32 arenas.
The thread is assigned based on the order modulo the number of cores. In CCIS machine, if the 33rd thread comes, it will be assigned to the 1st arena, and so on.
The arenas are created on the fly, depends on if another thread comes to request memory.

## Allocating memory

Allocating memory is handled by sbrk and mmap. For request larger than 4096 will be handled by mmap and will not using buddy system.
Meanwhile, for request smaller than 4096, we will use `sbrk` and use buddy system. The reason behind this, is to reduce the operation 
needed when requesting a memory.

## Data structure

I am using list, for the buddy system. In this implementation, we keep the header if someone free the memory, unless merge is possible.
That way, we don't need to create the header again when new request comes and fit the blocks.

Below is the information we keep in the struct:

```
typedef struct __block_header_t {
  void* address;                // the address of the data segment
  unsigned order;               // the order of the size
  size_t size;                  // size of data + size of header
  unsigned is_free;             // flag if data segment is free
  unsigned is_mmaped;           // if we use mmap
  struct __block_header_t *next;// next node
  unsigned __padding;           // bytes for padding
} block_header_t;
```

The header is located right before the data segment.

## Code overview

Basically, every functions has its own .c and .h file. The helper codes and data structure definition
is in the `mallutl.c` and `mallutl.h`.

# DESIGN DECISION

## Final design

1. The memory allocation is handled using mmap only.
2. The buddy allocation is handled by the block header only. The block header is located right before every data segment and connected to other block header in the linked list.
This way, we can always locate the header by simply substracting the data address by the size of the header.
Comparing to the initial design, in initial design, we have to search one by one in every arena and every header to find which header contains the address. This of course, has unnecessary complexity.

## Stats

In the stats, we follow `man mallinfo`. We consider the request above a page size as handled by mmap and less than one page is handled not by mmap. 

# RUNNING THE PROGRAM

1. `make clean`
2. `make check1`


# ADDITIONAL FEATURES

- [x] Implement malloc hook 
- [ ] Implement heap consistency checking

# KNOWN BUGS

1. Known bugs in the code are marked as `// FIXME`
2. Once or twice, the program would get segmentation fault, but runs fine afterwards. No clue on gdb
3. When compiled, gcc will show a warning. This is a [gcc bug](https://gcc.gnu.org/bugzilla/show_bug.cgi?id=53119)

*Tested on CCIS machine*
