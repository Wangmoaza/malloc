/*
 * mm-naive.c - The fastest, least memory-efficient malloc package.
 * 
 * Some parts of the code is modified from textbook source codes.
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
#define PRED(bp) ((char *)(bp))
#define SUCC(bp) ((char *)(bp) + WSIZE)
#define SET_PRED(bp, ptr) (*(unsigned int *)(bp) = (unsigned int)(ptr))
#define SET_SUCC(bp, ptr) (*((unsigned int *)(bp) + WSIZE) = (unsigned int)(ptr))

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)
#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))


/* helper functions */
static void *extend_heap(size_t words);
static void place(void *bp, size_t asize);
static void *coalesce(void *bp);
static void *find_fit(size_t asize);
static int mm_check(void);
static void printblock(void *bp);
static void checkblock(void *bp);
static void add_node(void * bp);
static void remove_node(void *bp);

/* Caution:
 * when using GET_ALLOC, GET_SIZE, inside should be HDRP or FTRP
 */

void * free_list;

static void add_node(void *bp)
{
    /* last-in fist-out */
    size_t size = GET_SIZE(HDRP(bp));   
    void *currp = free_list;

    if (currp == NULL) // list is empty
    {
        SET_PRED(bp, NULL);
        SET_SUCC(bp, NULL);
        free_list = bp;
    }

    else
    {
        SET_PRED(currp, bp);
        SET_PRED(bp, NULL);
        SET_SUCC(bp, currp);
        free_list = bp;
    }
}

static void remove_node(void *bp)
{
    size_t size = GET_SIZE(HDRP(bp));
    if (PRED(bp) == NULL) // bp is he
        ad of list
    {
        if (SUCC(bp) == NULL)
            free_list = NULL;
        else
        {
            SET_PRED(SUCC(bp), NULL);
            free_list = SUCC(bp);
        }
    }
    else
    {
        if (SUCC(bp) == NULL)
            SET_SUCC(PRED(bp), NULL);
        else
        {
            SET_PRED(SUCC(bp), PRED(bp));
            SET_SUCC(PRED(bp), SUCC(bp));
        }
    }
}

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

    if (GET_SIZE(HDRP(bp)) < 2 * DSIZE)
    {
        printf("Error: (%p) block size is less than 16 bytes\n", bp);
    }
}

/*
 * extend_heap - Extend heap with free block and return its block pointer
 */
static void *extend_heap(size_t words)
{
    char *bp;
    size_t asize;

    /* allocate even number of size to maintain alignment */
    asize = (words % 2) ? (words+1) * WSIZE : words * WSIZE;

    if ((long)(bp = mem_sbrk(asize)) == -1)  
        return NULL;

    /* Initialize free block header/footer and the epilogue header */
    PUT(HDRP(bp), PACK(asize, 0));         /* Free block header */
    PUT(FTRP(bp), PACK(asize, 0));         /* Free block footer */
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1)); /* New epilogue header */

    /* Coalesce if the previous block was free */
    bp = coalesce(bp);
    mm_check(); //FIXME delete this before submission
    return bp;
}

/*
 * coalesce - Boundary tag coalescing. Return ptr to coalesced block
 */
static void *coalesce(void *bp)
{
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    size_t size = GET_SIZE(HDRP(bp));

    /* case 1: prev - allocated, next - allocated */
    if (prev_alloc && next_alloc)
    {
        return bp;
    }
    /* case 2: prev - allocated, next - free */
    else if (prev_alloc && !next_alloc)
    {
        remove_node(bp);
        remove_node(NEXT_BLKP(bp));
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
    }
    /* case 3: prev - free, next - allocated */
    else if (!prev_alloc && next_alloc)
    {
        remove_node(bp);
        remove_node(PREV_BLKP(bp));
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }
    /* case 4: prev - free, next - free */
    else
    {
        remove_node(bp);
        remove_node(PREV_BLKP(bp));
        remove_node(NEXT_BLKP(bp));
        size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }

    add_node(bp);
    return bp;
}

/* 
 * place - Place block of asize bytes at start of free block bp 
 *         and split if remainder would be at least minimum block size
 */
static void place(void *bp, size_t asize)
{
    size_t csize = GET_SIZE(HDRP(bp)); // current block size
    void *remainp;
    size_t remain_size = csize - asize;

    if (remain_size >= (2 * DSIZE)) // split
    {
        remove_node(bp);
        PUT(HDRP(bp), PACK(asize, 1));
        PUT(FTRP(bp), PACK(asize, 1));

        /* remainder */
        remainp = NEXT_BLKP(bp);
        PUT(HDRP(remainp), PACK(remain_size, 0));
        PUT(FTRP(remainp), PACK(remain_size, 0));
        add_node(remainp);
    }

    else
    {
        remove_node(bp);
        PUT(HDRP(bp), PACK(csize, 1));
        PUT(FTRP(bp), PACK(czise, 1));
    }   
}

/* 
 * find_fit - Find a fit for a block with asize bytes
 * first fit search
 * return NULL if no fit found 
 */
static void *find_fit(size_t asize)
{
    void *currp = free_list;

    if (currp == NULL)
        return NULL;

    for (; SUCC(currp) != NULL; currp = SUCC(currp))
    {
        if (asize <= GET_SIZE(HDRP(currp)))
            return currp;
    }

    return NULL;
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
        int found = 0;

        if (ptr < heap_startp || ptr > heap_endp)
            printf("Error: (%p) out of bounds\n", ptr);

        if (GET_ALLOC(HDRP(ptr)) == 0 && GET_ALLOC(HDRP(NEXT_BLKP(ptr))) == 0)
            printf("Error: contiguous free blocks (%p) and (%p) escaped coalescing\n", ptr, NEXT_BLKP(ptr));

        /* If free block, is it in appropriate free list? */
        if (GET_ALLOC(HDRP(ptr)) == 0)
        {
            for (char * currp = free_list; SUCC(currp) != NULL; currp = SUCC(currp))
            {
                if (currp == ptr)
                {
                    found = 1;
                    break;
                }
            }

            if (!found)
                printf("Error: (%p) not in free list when it should be\n", ptr);                
        }
    }

    /* Is every block in the free list marked as free? */
    char * currp = free_list;
    for (; SUCC(currp) != NULL; currp = SUCC(currp))
    {
        if (GET_ALLOC(HDRP(currp)) != 0)
            printf("Error: allocated block (%p) is in free list\n");
    }
    

    return 1;
}
/* 
 * mm_init - initialize the malloc package.
 *     1. create initial empty heap area
 *     2. initialize empty free_list
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

    /* initialize empty free_list */
    free_list = NULL;

    mm_check();
    return 0;
}

/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size)
{
    printf("\nmm_malloc\n");
    mm_check();
    size_t asize;      /* Adjusted block size */
    size_t extendsize; /* Amount to extend heap if no fit */
    char *bp;      

    /* Ignore spurious requests */
    if (size == 0)
        return NULL;

    /* Adjust block size to include overhead and alignment reqs. */
    if (size <= DSIZE)
        asize = 2  *DSIZE;
    else
        asize = ALIGN(size + DSIZE);

    /* Search the free list for a fit */
    if ((bp = find_fit(asize)) != NULL) 
    {
        place(bp, asize);
        return bp;
    }

    /* No fit found. Get more memory and place the block */
    extendsize = MAX(asize,CHUNKSIZE);
    if ((bp = extend_heap(extendsize/WSIZE)) == NULL)  
        return NULL;
    
    place(bp, asize);
    mm_check();
    return bp;
}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *ptr)
{
    printf("\nmm_free\n");
    mm_check();
    if (ptr == 0) 
        return;

    size_t size = GET_SIZE(HDRP(bp));

    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));
    coalesce(bp);
    mm_check();
}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *ptr, size_t size)
{
    printf("\nmm_realloc\n");
    mm_check();

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
    mm_check();
}














