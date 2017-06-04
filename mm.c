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
#define PRED_PTR(bp) ((char *)(bp))
#define SUCC_PTR(bp) ((char *)(bp) + WSIZE)
#define PRED(bp) (*(char **)(bp))
#define SUCC(bp) (*(char **)(SUCC_PTR(bp)))
#define SET_PRED(bp, ptr) (*(unsigned int *)(PRED_PTR(bp)) = (unsigned int)(ptr))
#define SET_SUCC(bp, ptr) (*((unsigned int *)(SUCC_PTR(bp))) = (unsigned int)(ptr))

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
static void checkblock(void *bp);
static void add_node(void * bp);
static void remove_node(void *bp);
static void printblock(void *bp);
/* Caution:
 * when using GET_ALLOC, GET_SIZE, inside should be HDRP or FTRP
 */

void * free_list;
//static char *heap_startp = 0;

static void add_node(void *bp)
{
    printf("\nadd_node\n");
    /* last-in fist-out */
    //size_t size = GET_SIZE(HDRP(bp));   
    void *currp = free_list;

    if (free_list == NULL) // list is empty
    {
        printf("\tadd_node: list is empty\n");
        SET_PRED(bp, NULL);
        SET_SUCC(bp, NULL);
        free_list = bp;
        printf("\tadd_node: free_list head: [%p]\n", free_list);
    }

    else
    {
        SET_PRED(currp, bp);
        SET_PRED(bp, NULL);
        SET_SUCC(bp, currp);
        free_list = bp;
    }
    //mm_check();
    printf("---add_node\n");
}

static void remove_node(void *bp)
{
    printf("\nremove_node\n");
    //size_t size = GET_SIZE(HDRP(bp));
    if (PRED(bp) == NULL) // bp is head of list
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
        if (SUCC(bp) == NULL) // bp is tail of list
            SET_SUCC(PRED(bp), NULL);
        else
        {
            SET_PRED(SUCC(bp), PRED(bp));
            SET_SUCC(PRED(bp), SUCC(bp));
        }
    }
    //mm_check();
    printf("---remove_node\n");
}


/*
 * extend_heap - Extend heap with free block and return its block pointer
 */
static void *extend_heap(size_t words)
{
    printf("\nextend_heap\n");
    char *bp;
    size_t asize;

    /* allocate even number of size to maintain alignment */
    asize = (words % 2) ? (words+1) * WSIZE : words * WSIZE;

    if ((long)(bp = mem_sbrk(asize)) == -1)  
        return NULL;

    printf("\textend_heap: bp: [%p]\n", bp);
    /* Initialize free block header/footer and the epilogue header */
    PUT(HDRP(bp), PACK(asize, 0));         /* Free block header */
    PUT(FTRP(bp), PACK(asize, 0));         /* Free block footer */
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1)); /* New epilogue header */
    add_node(bp);
    /* Coalesce if the previous block was free */
    bp = coalesce(bp);
    mm_check(); //FIXME delete this before submission
    printf("---extend_heap\n");
    return bp;
}

/*
 * coalesce - Boundary tag coalescing. Return ptr to coalesced block
 */
static void *coalesce(void *bp)
{
    printf("\ncoalesce\n");
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    size_t size = GET_SIZE(HDRP(bp));

    /* case 1: prev - allocated, next - allocated */
    if (prev_alloc && next_alloc)
    {
        printf("\tcoalesce: case 1\n");
        printf("---coalesce\n");
        return bp;
    }
    /* case 2: prev - allocated, next - free */
    else if (prev_alloc && !next_alloc)
    {
        printf("\tcoalesce: case 2\n");
        remove_node(bp);
        remove_node(NEXT_BLKP(bp));
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
    }
    /* case 3: prev - free, next - allocated */
    else if (!prev_alloc && next_alloc)
    {
        printf("\tcoalesce: case 3\n");
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
        printf("\tcoalesce: case 4\n");
        remove_node(bp);
        remove_node(PREV_BLKP(bp));
        remove_node(NEXT_BLKP(bp));
        size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }

    add_node(bp);
    mm_check();
    printf("---coalesce\n");
    return bp;
}

/* 
 * place - Place block of asize bytes at start of free block bp 
 *         and split if remainder would be at least minimum block size
 */
static void place(void *bp, size_t asize)
{
    printf("\nplace\n");
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
        PUT(FTRP(bp), PACK(csize, 1));
    }
    mm_check();
    printf("---place\n");
}

/* 
 * find_fit - Find a fit for a block with asize bytes
 * first fit search
 * return NULL if no fit found 
 */
static void *find_fit(size_t asize)
{
    printf("\nfind_fit\n");
    void *currp = free_list;

    if (currp == NULL)
    {
        printf("---find_fit\n");
        return NULL;
    }

    for (; currp != NULL; currp = SUCC(currp))
    {
        if (asize <= GET_SIZE(HDRP(currp)))
        {
            printf("---find_fit\n");
            return currp;
        }
    }

    mm_check();
    printf("---find_fit\n");
    return NULL;
}

static void checkblock(void *bp)
{
    if ((size_t) bp % 8 != 0)
    {
        printf("\tError: (%p) not 8-byte aligned\n", bp);
    }

    if (GET(HDRP(bp)) != GET(FTRP(bp)))
    {
        printf("\tError: (%p) header and footer not identical\n", bp);
        printf("\theader: %x\tfooter:%x\n", GET(HDRP(bp)), GET(FTRP(bp)));
    }

    if (GET_SIZE(HDRP(bp)) < 2 * DSIZE)
    {
        printf("\tError: (%p) block size is %d\n", bp, GET_SIZE(HDRP(bp)));
    }
}

static void printblock(void *bp) 
{
    size_t hsize, halloc, fsize, falloc;

    hsize = GET_SIZE(HDRP(bp));
    halloc = GET_ALLOC(HDRP(bp));  
    fsize = GET_SIZE(FTRP(bp));
    falloc = GET_ALLOC(FTRP(bp));  

    if (hsize == 0) {
        printf("\t%p: EOL\n", bp);
        return;
    }

    printf("\t%p: header: [%u:%c] footer: [%u:%c]\n", bp, 
           hsize, (halloc ? 'a' : 'f'), 
           fsize, (falloc ? 'a' : 'f'));
    if (halloc == 0)
    {
        printf("\tpredecessor: [%p] successor: [%p]\n", PRED(bp), SUCC(bp));
    }
}

/*
 * mm_check - check heap corrrectness
 *     returns a nonzero value if heap is consistent
 */
static int mm_check(void)
{
    printf("\nmm_check\n");
    char *ptr;
    char *heap_startp = mem_heap_lo();
    char *heap_endp = mem_heap_hi();

    /* scan heap */
    for (ptr = heap_startp + 2*WSIZE; GET_SIZE(HDRP(ptr)) > 0; ptr = NEXT_BLKP(ptr))
    {
        printblock(ptr);
        checkblock(ptr);
        int found = 0;

        if (ptr < heap_startp || ptr > heap_endp)
            printf("\tError: (%p) out of bounds\n", ptr);

        if (GET_ALLOC(HDRP(ptr)) == 0 && GET_ALLOC(HDRP(NEXT_BLKP(ptr))) == 0)
            printf("\tError: contiguous free blocks (%p) and (%p) escaped coalescing\n", ptr, NEXT_BLKP(ptr));

        /* If free block, is it in appropriate free list? */
        if (GET_ALLOC(HDRP(ptr)) == 0)
        {
            //printf("\tptr: [%p] free_list head: [%p]\n", ptr, free_list);
            printf("\ttraversing free list\n");
            for (char * currp = free_list; currp != NULL; currp = SUCC(currp))
            {
                printblock(currp);
                if (currp == ptr)
                {
                    found = 1;
                    break;
                }
            }

            if (!found)
                printf("\tError: (%p) not in free list when it should be\n", ptr);                
        }
    }

    if (free_list == NULL)
    {
        printf("\tempty free list\n");
        printf("---mm_check\n");
        return 1;
    }

    /* Is every block in the free list marked as free? */
    for (char *currp = free_list; SUCC(currp) != NULL; currp = SUCC(currp))
    {
        //printf("\tcurrp: [%p] successor: [%p]\n", currp, SUCC(currp));
        if (GET_ALLOC(HDRP(currp)) != 0)
            printf("\tError: allocated block (%p) is in free list\n", currp);
    }

    printf("---mm_check\n");    
    return 1;
}
/* 
 * mm_init - initialize the malloc package.
 *     1. create initial empty heap area
 *     2. initialize empty free_list
 */
int mm_init(void)
{
    printf("\nmm_init\n");
    char * heap_startp;

    /* initialize empty free_list */
    free_list = NULL;
    
    /* create initial empty heap area */
    if ((heap_startp = mem_sbrk(4*WSIZE)) == (void *)-1)
        return -1;

    printf("heap_startp: %p\n", heap_startp);
    PUT(heap_startp, 0);                            /* alignment padding */
    PUT(heap_startp + (1*WSIZE), PACK(DSIZE, 1));   /* prologue header */
    PUT(heap_startp + (2*WSIZE), PACK(DSIZE, 1));   /* prologue footer */
    PUT(heap_startp + (3*WSIZE), PACK(0, 1));       /* epilogue header */
    
    mm_check();
    if (extend_heap(CHUNKSIZE/WSIZE) == NULL)
        return -1;



    mm_check();
    return 0;
}

/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size)
{
    printf("\nmm_malloc size: %u\n", size);
    printf("\tmm_malloc: before\n");
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
        printf("\tmm_malloc: after\n");
        mm_check();
        return bp;
    }

    /* No fit found. Get more memory and place the block */
    extendsize = MAX(asize,CHUNKSIZE);
    if ((bp = extend_heap(extendsize/WSIZE)) == NULL)  
        return NULL;
    
    place(bp, asize);
    printf("\tmm_malloc: after\n");
    mm_check();
    return bp;
}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *ptr)
{
    printf("\nmm_free ptr: %p\n", ptr);
    printf("\tmm_free: before:");
    mm_check();
    if (ptr == 0) 
        return;

    size_t size = GET_SIZE(HDRP(ptr));

    PUT(HDRP(ptr), PACK(size, 0));
    PUT(FTRP(ptr), PACK(size, 0));
    coalesce(ptr);
    printf("\tmm_free: after:");
    mm_check();
}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *ptr, size_t size)
{
    printf("\nmm_realloc ptr: %p size: %u\n", ptr, size);
    printf("\tmm_realloc: before\n");
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
    printf("\tmm_realloc: after\n");
    mm_check();
    return newptr;
}














