/*
 * mm-naive.c - The least memory-efficient malloc package.
 * 
 * In this naive approach, a block is allocated by allocating a
 * new page as needed.  A block is pure payload. There are no headers or
 * footers.  Blocks are never coalesced or reused.
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


/* always use 16-byte alignment */
#define ALIGNMENT 16


/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~(ALIGNMENT-1))


/* rounds up to the nearest multiple of mem_pagesize() */
#define PAGE_ALIGN(size) (((size) + (mem_pagesize()-1)) & ~(mem_pagesize()-1))


//chunk size = 4 pages
#define CHUNK_SIZE (1 << 14) //4 pages


//how much memory we have to add to every request to get actual block size
#define OVERHEAD (sizeof(block_header) + sizeof(block_footer))


//get to header and footer macro
#define HDRP(bp) ((char*)(bp) - sizeof(block_header))
#define FTRP(bp) ((char*)(bp) + GET_SIZE(HDRP(bp)) - OVERHEAD)


//get next block header (next block's payload)
//go from payload to payload pointer
#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)))
#define PREV_BLKP(bp) ((char *)(bp) - GET_SIZE((char *)(bp) - OVERHEAD))

#define CHUNK_ALIGN(size) (((size) + (CHUNK_SIZE-1)) & ~(CHUNK_SIZE - 1))

//cast generic poiter to size t
#define GET(p) (*(size_t *)(p))
#define GET_ALLOC(p) (GET(p) & 0x1)
#define GET_SIZE(p) (GET(p) & ~0xF)


#define PUT(p,val) (*(size_t *)(p) = (val))
#define PACK(size, alloc) ((size) | (alloc))


#define NEXT_FREE (((list_node *)free_list)->next)

void extend(size_t new_size);
void set_allocated(void *bp, size_t size);
void *coalesce(void *bp);
void *temp_pointer;

//linked list strucutre
typedef struct list_node {
	struct list_node *prev;
	struct list_node *next;
} list_node;

typedef struct page_node {
	struct page_node *prev;
	struct page_node *next;
	size_t size;
} page_node;


//header and footer
typedef size_t block_header;
typedef size_t block_footer;
typedef size_t page_size;

//pointer to list of free mem
list_node *free_list;
void *page_list;

/*
 *
 * extend - new size: amount of memory to allocate per page
 * have big enough block to allocate from
 *
 */

//TODO need to have a null pointer for the explicit list to check so that it can end the while loop and extend memory

void extend(size_t new_size) {
  //allocate 4 pages of requested size 	memory
  size_t chunk_size = CHUNK_ALIGN(new_size);
  void *new_page = mem_map(chunk_size);
  page_node *new_page_node;
  new_page_node = new_page;

  PUT(new_page + sizeof(list_node), chunk_size);

  PUT(new_page, new_page_node);

  int x;
  x = sizeof(new_page_node);
  if(page_list == NULL)
  {
	  page_node *page_list = new_page_node;
	  new_page_node->next = NULL;
	  new_page_node->prev = NULL;
  }


  //set first 8 bytes to null
  void *page_pointer = new_page + sizeof(new_page_node);

  //Set prologue - 16 bytes (8 for header, 8 for footer)
  PUT(page_pointer, PACK(16,1));
  PUT(page_pointer + sizeof(block_header), PACK(16,0));

  //update page pointer
  page_pointer = (char *)page_pointer + OVERHEAD;

  //chunk size
  size_t free_space = chunk_size - (OVERHEAD << 1);

  //create header and footer for free space
  PUT(page_pointer, PACK(free_space,0));
  PUT(page_pointer + free_space - sizeof(block_footer), PACK(free_space,0));

  //update page pointer past header
  page_pointer = (char *)page_pointer + 8 ;

  //update list_node
  list_node *list = page_pointer;
  //check if free page the first
  if(free_list != NULL)
  {

	  list->next = free_list;
	  ((list_node *)free_list)->prev = list;

  }
  else
  {
	  list->prev = NULL;
	  list->next = NULL;

  }
  //update head pointer

  // TODO currently stamping data, need to append pointer
  free_list = list;

  //move page pointer to page terminator
  page_pointer = (char *)page_pointer + free_space - sizeof(block_footer);

  //end page with 0
  PUT(page_pointer, PACK(0,1));
}


//change unallocated bit to allocated
//also does work of splitting
void set_allocated(void *bp, size_t size){

	size_t free_size = GET_SIZE(HDRP(bp)) - size;

	//check if there is not enough free space for a overhead plus free space
	if(free_size <= (OVERHEAD + sizeof(list_node)))
	{
		//mark header and footer as allocated
		PUT(HDRP(bp), PACK(size, 1));
		PUT(FTRP(bp), PACK(size, 0));

		//update free space list pointer
		list_node *n = ((list_node*)bp)->next;
		list_node *p = ((list_node*)bp)->prev;

		if(n != NULL){
			n->prev = p;
		}

		if(p != NULL){
			p->next = n;
		}

		if(bp == free_list){
			if(((list_node *)bp)->next != NULL){
				free_list = ((list_node *)bp)->next;
			}
			else{
				free_list = NULL;
			}
		}
		temp_pointer = bp;
	}
	else{
		int x;
		x = OVERHEAD;
		//mark header and footer as allocated
		void* new_footer = (char *)bp + size - OVERHEAD;
		void* new_header = (char *)new_footer + sizeof(block_footer);
		PUT(HDRP(bp), PACK(size, 1));
		PUT(new_footer, PACK(size, 0));

		//update header and footer of free space
		PUT(new_header, PACK(free_size, 0));
		PUT(FTRP(bp), PACK(free_size, 0));


		//if at end of list
		void* node_pointer = (char *)new_header + sizeof(block_header);
		list_node *new_node = node_pointer;
		list_node *old_node = bp;
		//TODO: maybe fix list_node pointer
		PUT(node_pointer, new_node);

		//first case

		free_list = new_node;
		new_node->prev = old_node->prev;
		new_node->next = old_node->next;

		if(old_node->next != NULL)
		{
			old_node->next->prev = new_node;
		}
		if(old_node->prev != NULL)
		{
			old_node->prev->next = new_node;
		}

		//test get old header and footer
		size_t z;

		z = GET_SIZE(HDRP(PREV_BLKP(old_node)));



		temp_pointer = bp;
	}
}




//void *coalesce(void *bp) {
//
//	//TODO check either direction. if 0, set that
//	void* test;
//	test = NEXT_BLKP((HDRP(bp)));
//	int next_alloc;
//	next_alloc = GET_ALLOC(test);
//
//	test = PREV_BLKP((HDRP(bp)));
//	int prev_alloc;
//	prev_alloc = GET_ALLOC(test);
//
//
//  size_t size = GET_SIZE(HDRP(bp));
//
//
//  if(prev_alloc && next_alloc){
//   //do nothing
//
// }
// else if(prev_alloc && !next_alloc){
//   size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
//   PUT(HDRP(bp), PACK(size, 0));
//   PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
// }
// else if (!prev_alloc && next_alloc) {
//	 size_t s;
//	 void* block;
//	 void* header;
//	 block = PREV_BLKP(bp);
//	 header = HDRP(block);
//	 s = (*(size_t *)header) & ~0xf;
//   size += s;
//   PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
//   PUT(FTRP(bp), PACK(size, 0));
//   bp = PREV_BLKP(bp);
// }
//
// else {
//   size += (GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(HDRP(NEXT_BLKP(bp))));
//   PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
//   PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
//   bp = PREV_BLKP(bp);
// }
//
//  return bp;
//}


/*
 *
 * mm_init - initialize the malloc package.
 * Allocating the intial heap area.
 *
 */
int mm_init(void) {
  //reset implementation
	free_list = NULL;
	page_node *newnode;
//	if(page_list != NULL)
//	{
//
//
//	}

  //allocate initial heap area
  extend(mem_pagesize());

  return 0;
}


/*
 * mm_malloc - Allocate a block by using bytes from current_avail,
 *     grabbing a new page if necessary.
 * Returns a pointer to an allocated block paylod
 */
void *mm_malloc(size_t size) {
  //int need_size = max(size, sizeof(list_node));
  int new_size = ALIGN(size + OVERHEAD);
  void *bp = free_list;


  //first fit
  while(GET_SIZE(HDRP(bp)) != 0)  {
	  //for debugging

    if(!GET_ALLOC(HDRP(bp)) && (GET_SIZE(HDRP(bp)) >= new_size)){
    	  int x;
    	  x = GET_SIZE(HDRP(bp));
      set_allocated(bp, new_size);
      return bp;
    }
    if(((list_node *)bp)->next != NULL)
      bp = ((list_node *)bp)->next;
    else
      break;
  }


  //extend only if we're out of room!
   extend(new_size);
   void* temp = free_list;
   set_allocated(free_list, new_size);
   return temp;
}



/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *bp)
{
	list_node * test = bp;
	int free_size = GET_SIZE(HDRP(bp));
	PUT(HDRP(bp), PACK(free_size, 0));
	PUT(FTRP(bp), PACK(free_size, 0));

	//look at

	list_node *new_node;
	new_node = (bp);
	list_node *old_node;
	if (free_list != NULL) {
//		PUT(bp, new_node);
		old_node = free_list;
		//account for previous values
		//inserts the new node in between if in the middle of list
		new_node->next = old_node;
		old_node->prev = new_node;
		new_node->prev = NULL;

		free_list = new_node;

	}
	else {
//		PUT(bp, new_node);

		new_node->prev = NULL;
		new_node->next = NULL;
		free_list = new_node;

	}
//	void* returnfree;
//	coalesce(bp);
//
//
}




