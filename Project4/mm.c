/*
 * mm-naive.c - The fastest, least memory-efficient malloc package.
 * 
 * In this naive approach, a block is allocated by simply incrementing
 * the brk pointer.  A block is pure payload. There are no headers or
 * footers.  Blocks are never coalesced or reused. Realloc is
 * implemented directly using mm_malloc and mm_free.
 *
 * NOTE TO STUDENTS: Replace this header comment with your own header
 * comment that gives a high level description of your solution.
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include "mm.h"
#include "memlib.h"

/* $begin mallocmacros */
/* Basic constants and macros */
#define WSIZE       4       /* Word and header/footer size (bytes) */ 
#define DSIZE       8       /* Double word size (bytes) */
#define CHUNKSIZE  (1<<12)  /* Extend heap by this amount (bytes) */
#define LISTSIZE    16      /* Segregated list size (total 16 lists) */

#define MAX(x, y) ((x) > (y)? (x) : (y))  

/* Pack a size and allocated bit into a word */
#define PACK(size, alloc)  ((size) | (alloc)) 

/* Read and write a word at address p */
#define GET(p)       (*(unsigned int *)(p))           
#define PUT(p, val)  (*(unsigned int *)(p) = (val))   

/* Read the size and allocated fields from address p */
#define GET_SIZE(p)  (GET(p) & ~0x7)                   
#define GET_ALLOC(p) (GET(p) & 0x1)                    

/* Given block ptr bp, compute address of its header and footer */
#define HDRP(bp)       ((char *)(bp) - WSIZE)                     
#define FTRP(bp)       ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE) 

/* Given block ptr bp, compute address of next and previous blocks */
#define NEXT_BLKP(bp)  ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE))) 
#define PREV_BLKP(bp)  ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE))) 
/* $end mallocmacros */

#define PREV_FREE(bp)   (*(void **)(bp))
#define NEXT_FREE(bp)   (*(void **)(bp + WSIZE))

/* Global variables */
static char *heap_listp = 0;  /* Pointer to first block */
static void *free_list_0, *free_list_1, *free_list_2, *free_list_3, *free_list_4, *free_list_5, *free_list_6, *free_list_7, *free_list_8,
            *free_list_9, *free_list_10, *free_list_11, *free_list_12, *free_list_13, *free_list_14, *free_list_15;


/* Function prototypes for internal helper routines */
static void *extend_heap(size_t words);
static void place(void *bp, size_t asize);
static void *find_fit(size_t asize);
static void *coalesce(void *bp);
static void printblock(void *bp); 
static void checkheap(int verbose);
static void checkblock(void *bp);
static void insert_free_list(char *bp, size_t size);
static void remove_free_list(char *bp);
void        mm_check(int verbose);

/*********************************************************
 * NOTE TO STUDENTS: Before you do anything else, please
 * provide your information in the following struct.
 ********************************************************/
team_t team = {
    /* Your student ID */
    "20192132",
    /* Your full name*/
    "Jahong Koo",
    /* Your email address */
    "mongoo8947@sogang.ac.kr",
};

/* 
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{
    void    **free_list[LISTSIZE] = { &free_list_0, &free_list_1, &free_list_2, &free_list_3, &free_list_4, &free_list_5, &free_list_6, &free_list_7,
                                    &free_list_8, &free_list_9, &free_list_10, &free_list_11, &free_list_12, &free_list_13, &free_list_14, &free_list_15 };
    /* Create the initial empty heap */
    if ((heap_listp = mem_sbrk(4 * WSIZE)) == (void *)-1)
        return -1;

    PUT(heap_listp, 0);                          /* Alignment padding */
    PUT(heap_listp + (1 * WSIZE), PACK(DSIZE, 1)); /* Prologue header */ 
    PUT(heap_listp + (2 * WSIZE), PACK(DSIZE, 1)); /* Prologue footer */ 
    PUT(heap_listp + (3 * WSIZE), PACK(0, 1));     /* Epilogue header */
    heap_listp += (2 * WSIZE);                      

    for (int i = 0; i < LISTSIZE; i++)
        *free_list[i] = NULL;

    /* Extend the empty heap with a free block of CHUNKSIZE bytes */
    if (extend_heap(CHUNKSIZE) == NULL) 
        return -1;
    return 0;
}

/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size)
{
    size_t  asize;      /* Adjusted block size */
    size_t  extendsize; /* Amount to extend heap if no fit */
    char    *bp;      

    if (heap_listp == 0)
        mm_init();

    /* Ignore spurious requests */
    if (size == 0)
        return NULL;

    /* Adjust block size to include overhead and alignment reqs. */
    if (size <= DSIZE)                                          
        asize = 2 * DSIZE;                                      
    else
        asize = DSIZE * ((size + (DSIZE) + (DSIZE - 1)) / DSIZE);

    /* Search the free list for a fit */
    if ((bp = find_fit(asize)) != NULL)
    {  
        place(bp, asize);                  
        return bp;
    }

    /* No fit found. Get more memory and place the block */
    extendsize = MAX(asize, CHUNKSIZE);                
    if ((bp = extend_heap(extendsize)) == NULL)  
        return NULL;                                  
    place(bp, asize);                                 
    return bp;
}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *bp)
{
    size_t  size;

    if (bp == NULL) 
        return;

    if (heap_listp == 0)
        mm_init();

    size = GET_SIZE(HDRP(bp));
    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));
    insert_free_list(bp, size);
    coalesce(bp);
    //mm_check(0);
}

/*
 * coalesce - Boundary tag coalescing. Return ptr to coalesced block
 */
static void *coalesce(void *bp) 
{
    size_t  prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
    size_t  next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    size_t  size = GET_SIZE(HDRP(bp));

    /* Case 1 : both prev and next allocated */
    if (prev_alloc && next_alloc)
    {
        return bp;
    }
    /* Case 2 : next block not allocated */
    else if (prev_alloc && !next_alloc)
    {
        remove_free_list(bp);
        remove_free_list(NEXT_BLKP(bp));
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
    }
    /* Case 3 : prev block not allocated */
    else if (!prev_alloc && next_alloc)
    {
        remove_free_list(bp);
        remove_free_list(PREV_BLKP(bp));
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        PUT(FTRP(bp), PACK(size, 0));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }
    /* Case 4 : both prev and next not allocated */
    else
    {
        remove_free_list(bp);
        remove_free_list(PREV_BLKP(bp));
        remove_free_list(NEXT_BLKP(bp));                             
        size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(FTRP(NEXT_BLKP(bp)));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }

    insert_free_list(bp, size);
    return bp;
}

/*
 * mm_realloc - Naive implementation of realloc
 */
void *mm_realloc(void *bp, size_t size)
{
    size_t  oldsize;
    size_t  newsize;
    size_t  remainder;
    void    *newbp;

    /* if size is 0, then this is just free */
    if (size == 0)
    {
        mm_free(bp);
        return NULL;
    }

    /* if bp is NULL, then this is just malloc */
    if (bp == NULL)
        return mm_malloc(size);

    /* Adjust block size to include overhead and alignment reqs. */
    if (size <= DSIZE)                                          
        newsize = 2 * DSIZE;                                      
    else
        newsize = DSIZE * ((size + (DSIZE) + (DSIZE - 1)) / DSIZE);
    
    /* if oldsize is bigger than or equl to newsize, do nothing */
    oldsize = GET_SIZE(HDRP(bp));
    if (newsize <= oldsize)
    {
        return bp;
    }
    
    /* else, extend the block */
    /* check extendability on current block address */
    if (!GET_ALLOC(HDRP(NEXT_BLKP(bp))))
    {
        remainder = GET_SIZE(HDRP(bp)) + GET_SIZE(HDRP(NEXT_BLKP(bp))) - newsize;
        if (remainder < 0)
        {
            if (extend_heap(MAX(CHUNKSIZE, (-1) * remainder)) == NULL)
                return NULL;
            remainder += MAX(CHUNKSIZE, (-1) * remainder);
        }
        remove_free_list(NEXT_BLKP(bp));
        PUT(HDRP(bp), PACK(newsize + remainder, 1));
        PUT(FTRP(bp), PACK(newsize + remainder, 1));
        return bp;
    }

    /* if not extendable, allocate new block */
    newbp = mm_malloc(size);
    if (newbp == NULL)
        return NULL;
    memcpy(newbp, bp, oldsize - DSIZE);
    mm_free(bp);
    return newbp;
}

/* 
 * mm_check - Check the heap for correctness
 */
void mm_check(int verbose)  
{ 
    //printf("Checking Heap Consistency...\n");
    checkheap(verbose);
}

/* 
 * extend_heap - Extend heap with free block and return its block pointer
 */
/* $begin mmextendheap */
static void *extend_heap(size_t words) 
{
    char    *bp;
    size_t  size;

    /* Allocate an even number of words to maintain alignment */
    size = (size_t)(words + DSIZE - 1) & ~0x7;
    if ((bp = mem_sbrk(size)) == (void *)-1)  
        return NULL;                                        

    /* Initialize free block header/footer and the epilogue header */
    PUT(HDRP(bp), PACK(size, 0));         /* Free block header */   
    PUT(FTRP(bp), PACK(size, 0));         /* Free block footer */  
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1)); /* New epilogue header */
    insert_free_list(bp, size);

    /* Coalesce if the previous block was free */
    return coalesce(bp);                                         
}

/* 
 * place - Place block of asize bytes at start of free block bp 
 *         and split if remainder would be at least minimum block size
 */
static void place(void *bp, size_t asize)
{
    size_t  csize = GET_SIZE(HDRP(bp));   

    remove_free_list(bp);
    /* Remainder is larger than or equal to minimum block size */
    if ((csize - asize) >= (2 * DSIZE))
    {
        /* Modify header and footer of allocating block */
        PUT(HDRP(bp), PACK(asize, 1));
        PUT(FTRP(bp), PACK(asize, 1));
        /* Modify header and Footer of splitted free block */
        bp = NEXT_BLKP(bp);
        PUT(HDRP(bp), PACK(csize - asize, 0));
        PUT(FTRP(bp), PACK(csize - asize, 0));

        insert_free_list(bp, csize - asize);
        coalesce(bp);
    }
    /* Remainder is smaller than minimum block size -> No splitting */
    else
    {
        /* Modify header and Footer of allocating block */
        PUT(HDRP(bp), PACK(csize, 1));
        PUT(FTRP(bp), PACK(csize, 1));
    }

    //mm_check(0);
}

/* 
 * find_fit - Find a fit for a block with asize bytes 
 */
static void *find_fit(size_t asize)
{
    void    *bp;
    void    **free_list[LISTSIZE] = { &free_list_0, &free_list_1, &free_list_2, &free_list_3, &free_list_4, &free_list_5, &free_list_6, &free_list_7,
                                    &free_list_8, &free_list_9, &free_list_10, &free_list_11, &free_list_12, &free_list_13, &free_list_14, &free_list_15 };

    for (int i = 0; i < LISTSIZE; i++)
    {
        if (*free_list[i] == NULL)
            continue;
        if ((asize <= (1 << (i + 4))) || i == LISTSIZE - 1)
        {
            bp = *free_list[i];
            while (bp != NULL && (asize > GET_SIZE(HDRP(bp))))
                bp = NEXT_FREE(bp);
            if (bp != NULL)
                return bp;
        }
    }

    return NULL;  /* no fit found */
}

static void printblock(void *bp) 
{
    size_t  hsize, halloc, fsize, falloc;

    checkheap(0);
    hsize = GET_SIZE(HDRP(bp));
    halloc = GET_ALLOC(HDRP(bp));  
    fsize = GET_SIZE(FTRP(bp));
    falloc = GET_ALLOC(FTRP(bp));  

    if (hsize == 0)
    {
        printf("%p: EOL\n", bp);
        return;
    }

    printf("%p: header: [%ld:%c] footer: [%ld:%c]\n", bp, hsize, (halloc ? 'a' : 'f'), fsize, (falloc ? 'a' : 'f')); 
}

static void checkblock(void *bp) 
{
    if (bp == NULL) {
        printf("Error: NULL block pointer\n");
        return;
    }
    if ((size_t)bp % 8) {
        printf("Error: %p is not doubleword aligned\n", bp);
    }
    if (GET(HDRP(bp)) != GET(FTRP(bp))) {
        printf("Error: header does not match footer\n");
    }
}

/* 
 * checkheap - Minimal check of the heap for consistency 
 */
void checkheap(int verbose) 
{
    char    *bp = heap_listp;
    int     free_block_count = 0;
    int     free_list_count = 0;
    void    **free_list[LISTSIZE] = { &free_list_0, &free_list_1, &free_list_2, &free_list_3, &free_list_4, &free_list_5, &free_list_6, &free_list_7,
                                    &free_list_8, &free_list_9, &free_list_10, &free_list_11, &free_list_12, &free_list_13, &free_list_14, &free_list_15 };

    if (verbose)
        printf("Heap (%p):\n", heap_listp);

    if ((GET_SIZE(HDRP(heap_listp)) != DSIZE) || !GET_ALLOC(HDRP(heap_listp)))
        printf("Bad prologue header\n");
    checkblock(heap_listp);

    for (bp = heap_listp; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp))
    {
        if (verbose) 
            printblock(bp);
        checkblock(bp);

        // Check if every block in the free list is marked as free
        if (!GET_ALLOC(HDRP(bp))) {
            free_block_count++;
        }

        // Check if there are contiguous free blocks that somehow escaped coalescing
        if (!GET_ALLOC(HDRP(bp)) && !GET_ALLOC(HDRP(NEXT_BLKP(bp)))) {
            printf("Error: Contiguous free blocks escaped coalescing at %p\n", bp);
        }
    }

    if (verbose)
        printblock(bp);
    if ((GET_SIZE(HDRP(bp)) != 0) || !(GET_ALLOC(HDRP(bp))))
        printf("Bad epilogue header\n");

    // Check if every free block is actually in the free list
    for (int i = 0; i < LISTSIZE; i++) {
        for (bp = *free_list[i]; bp != NULL; bp = NEXT_FREE(bp)) {
            free_list_count++;

            // Check if pointers in the free list point to valid free blocks
            if (GET_ALLOC(HDRP(bp))) {
                printf("Error: Allocated block in free list at %p\n", bp);
            }

            // Check if the block is within the heap bounds
            if ((char *)bp < mem_heap_lo() || (char *)bp > mem_heap_hi()) {
                printf("Error: Free list pointer out of heap bounds at %p\n", bp);
            }
        }
    }

    // Compare free block count in heap with free list count
    if (free_block_count != free_list_count) {
        printf("Error: Mismatch between free block count in heap and free list\n");
    }

    // Check for overlapping allocated blocks
    for (bp = heap_listp; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp)) {
        if (GET_ALLOC(HDRP(bp))) {
            char *next_bp = NEXT_BLKP(bp);
            if (next_bp < mem_heap_hi() && GET_ALLOC(HDRP(next_bp)) && (bp + GET_SIZE(HDRP(bp)) > next_bp)) {
                printf("Error: Allocated blocks overlap between %p and %p\n", bp, next_bp);
            }
        }
    }
}

/*
 * insert_free_list 
 */
static void insert_free_list(char *bp, size_t size)
{
    void    *cur;
    void    *prev;
    int     idx;
    void    **free_list[LISTSIZE] = { &free_list_0, &free_list_1, &free_list_2, &free_list_3, &free_list_4, &free_list_5, &free_list_6, &free_list_7,
                                    &free_list_8, &free_list_9, &free_list_10, &free_list_11, &free_list_12, &free_list_13, &free_list_14, &free_list_15 };

    /* Find index of free_list by size */
    for (idx = 0; idx < LISTSIZE - 1; idx++)
    {
        if (size <= (1 << (idx + 4)))
            break;
    }
    
    /* Find position in list */
    cur = *free_list[idx];
    prev = NULL;
    while ((cur != NULL) && (size > GET_SIZE(HDRP(cur))))
    {
        prev = cur;
        cur = NEXT_FREE(cur);
    }

    if (cur != NULL)
    {
        /* between begin and end */
        if (prev != NULL)
        {
            NEXT_FREE(bp) = cur;
            PREV_FREE(bp) = prev;
            PREV_FREE(cur) = bp;
            NEXT_FREE(prev) = bp;
        }
        /* begin */
        else
        {
            NEXT_FREE(bp) = cur;
            PREV_FREE(bp) = NULL;
            PREV_FREE(cur) = bp;
            *free_list[idx] = bp;
        }
    }
    else
    {
        /* end */
        if (prev != NULL)
        {
            NEXT_FREE(bp) = NULL;
            PREV_FREE(bp) = prev;
            NEXT_FREE(prev) = bp;
        }
        /* empty */
        else
        {
            PREV_FREE(bp) = NULL;
            NEXT_FREE(bp) = NULL;
            *free_list[idx] = bp;
        }
    }
}

/*
 * remove_free_list 
 */
static void remove_free_list(char *bp)
{
    size_t  size;
    int     idx;
    void    **free_list[LISTSIZE] = { &free_list_0, &free_list_1, &free_list_2, &free_list_3, &free_list_4, &free_list_5, &free_list_6, &free_list_7,
                                    &free_list_8, &free_list_9, &free_list_10, &free_list_11, &free_list_12, &free_list_13, &free_list_14, &free_list_15 };

    size = GET_SIZE(HDRP(bp));
    /* Find index of free_list by size */
    for (idx = 0; idx < LISTSIZE - 1; idx++)
    {
        if (size <= (1 << (idx + 4)))
            break;
    }

    /* Find position */
    if (NEXT_FREE(bp) != NULL)
    {
        /* between begin and end */
        if (PREV_FREE(bp) != NULL)
        {
            PREV_FREE(NEXT_FREE(bp)) = PREV_FREE(bp);
            NEXT_FREE(PREV_FREE(bp)) = NEXT_FREE(bp);
        }
        /* begin */
        else
        {
            PREV_FREE(NEXT_FREE(bp)) = NULL;
            *free_list[idx] = NEXT_FREE(bp);
        }
    }
    else
    {
        /* end */
        if (PREV_FREE(bp) != NULL)
            NEXT_FREE(PREV_FREE(bp)) = NULL;
        /* empty */
        else
            *free_list[idx] = NULL;
    }
}