/*
 * mm-naive.c - The fastest, least memory-efficient malloc package.
 * 
 * In this naive approach, a block is allocated by simply incrementing
 * the brk pointer.  A block is pure payload. There are no headers or
 * footers.  Blocks are never coalesced or reused. Realloc is
 * implemented directly using mm_malloc and mm_free.
 *
 * NOTE TO STUDENTS:  Replace this header comment with your own header
 * comment that gives a high level description of your solution.
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include "mm.h"
#include "memlib.h"

/*********************************************************
 * NOTE TO STUDENTS: Before you do anything else, please
 * provide your team information in the following struct.
 ********************************************************/
team_t team = {
    /* Team name */
    "2013-10892",
    /* First member's full name */
    "Ha-Eun Hwangbo",
    /* First member's email address */
    "haeun0512@snu.ac.kr",
    /* Second member's full name (leave blank if none) */
    "",
    /* Second member's email address (leave blank if none) */
    ""
};

/* $begin mallocmacros */
/* Basic constants and macros from CSAPP textbook */
#define WSIZE       4       /* Word and header/footer size (bytes) */
#define DSIZE       8       /* Double word size (bytes) */
#define CHUNKSIZE  (1<<12)  /* Extend heap by this amount (bytes) */

#define MAX(x, y) ((x) > (y)? (x) : (y))  

/* Pack a size and allocated bit into a word */
#define PACK(size, alloc)  ((size) | (alloc))

/* Read and write a word at address p */
#define GET(p)       (*(unsigned int *)(p))           
#define PUT(p, val)  (*(unsigned int *)(p) = (val))

/* Read the size and allocated fields from address p */
#define GET_SIZE(p)  (GET(p) & ~0x7)
#define GET_ALLOC(p) (GET(p) & 0x1)

/* block pointer points to the first payload byte */
/* Given block ptr bp, compute address of its header and footer */
#define HDRP(bp)       ((char *)(bp) - WSIZE)
#define FTRP(bp)       ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)

/* Given block ptr bp, compute address of next and previous (physical) blocks */
#define NEXT_BLKP(bp)  ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE)))
#define PREV_BLKP(bp)  ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE)))
/* $end mallocmacros */

/* Given block ptr bp, go to next and previous block in free list by pointer */
#define NEXT_LIST(bp) //TODO implement this
#define PREV_LIST(bp) //TODO implement this
 
/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)
#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

#define MAX_LIST = 20

/* global variables */
/* array of segregation lists by power of 2 starting from 16
 * [0] : 0 - 16
 * [1] : 17 - 32
 * [2] : 33 - 64
 * ...
 */
void *free_lists_arr[MAX_LIST]; 

/* helper functions */
static void *extend_heap(size_t words);
static void place(void *bp, size_t asize);
static void *coalesce(void *bp);
static int mm_check(void);
static void printblock(void *bp);
static void checkheap(int verbose);
static void checkblock(void *bp);

/* Caution:
 * when using GET_ALLOC, GET_SIZE, inside should be HDRP or FTRP
 */

static void checkblock(void *bp)
{
    if ((size_t) bp % 8 != 0)
    {
        printf("Error: (%p) not 8-byte aligned\n", bp);
    }

    if (GET(HDRP(bp)) != GET(FTRP(bp)))
    {
        printf("Error: (%p) header and footer not identical\n", bp);
    }

    if (GET_SIZE(HDPR(bp)) < 2 * DSIZE)
    {
        printf("Error: (%p) block size is less than 16 bytes\n", bp);
    }
}

/*
 * mm_check - check heap corrrectness
 *     returns a nonzero value if heap is consistent
 */
static int mm_check(void)
{
    char *ptr;
    int list_idx = 0;

    size_t *heap_startp = mem_heap_lo();
    size_t *heap_endp = mem_heap_hi();
    size_t *curr_blockp = heap_startp;

    /* scan heap */
    for (ptr = heap_startp; GET_SIZE(HDRP(ptr)) > 0; ptr = NEXT_BLKP(ptr))
    {
        checkblock(ptr);
        
        if (ptr < heap_startp || ptr > heap_endp)
            printf("Error: (%p) out of bounds\n", ptr);

        if (GET_ALLOC(HDPR(ptr)) == 0 && GET_ALLOC(HDPR(NEXT_BLKP(ptr))) == 0)
            printf("Error: contiguous free blocks (%p) and (%p) escaped coalescing\n", ptr, NEXT_BLKP(ptr));

        /* If free block, is it in appropriate free list? */
        if (GET_ALLOC(HDPR(ptr)) == 0)
        {
            int i = 0;
            int found_flag = 0;
            size_t search_size = GET_SIZE(HDRP(ptr));
            for (int i = 0; i < MAX_LIST; i++)
            {
                if (free_lists_arr[i] == NULL) // list is empty
                {
                    search_size >>= 1; // go to next bigger list
                    continue;
                }

                //FIXME start from here
                search_size >>= 1; // go to next search size (* 2)
            }
        }

    }

    return 1;
}
/* 
 * mm_init - initialize the malloc package.
 *     1. create initial empty heap area
 *     2. initialize empty free_lists_arr
 */
int mm_init(void)
{
    int list_idx;
    char * heap_startp;

    /* create initial empty heap area */
    if ((heap_startp = mem_sbrk(4*WSIZE)) == (void *)-1)
        return -1;

    PUT(heap_startp, 0);                            /* alignment padding */
    PUT(heap_startp + (1*WSIZE), PACK(DSIZE, 1));   /* prologue header */
    PUT(heap_startp + (2*WSIZE), PACK(DSIZE, 1));   /* prologue footer */
    PUT(heap_startp + (3*WSIZE), PACK(0, 1));       /* epilogue header */
    
    if (extend_heap(CHUNKSIZE/WSIZE) == NULL)
        return -1;

    /* initialize empty free_lists_arr */
    for (list_idx = 0; list_idx < MAX_LIST; list_idx++)
        free_lists_arr[list_idx] = NULL

    return 0;
}

/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size)
{
    int newsize = ALIGN(size + SIZE_T_SIZE);
    void *p = mem_sbrk(newsize);
    if (p == (void *)-1)
	return NULL;
    else {
        *(size_t *)p = size;
        return (void *)((char *)p + SIZE_T_SIZE);
    }
}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *ptr)
{
}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *ptr, size_t size)
{
    void *oldptr = ptr;
    void *newptr;
    size_t copySize;
    
    newptr = mm_malloc(size);
    if (newptr == NULL)
      return NULL;
    copySize = *(size_t *)((char *)oldptr - SIZE_T_SIZE);
    if (size < copySize)
      copySize = size;
    memcpy(newptr, oldptr, copySize);
    mm_free(oldptr);
    return newptr;
}














