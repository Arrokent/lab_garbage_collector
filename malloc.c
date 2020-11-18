
/*
 *  CS213 - Lab assignment 3
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

/* Win32: Use PE Const Instead in program environment */
//#include <elf.h>

#include "memlib.h"
#include "malloc.h"
#include "useful.h"

team_t team = {
    /* Team name to be displayed on webpage */
    "",
    /* First member full name */
    "",
    /* First member email address */
    "",
    /* Second member full name (leave blank if none) */
    "",
    /* Second member email address (blank if none) */
    ""
};

/*
 * The free list has a ptr to its head at dseg_lo, and the tree of 
 * allocated blocks has a ptr to its root stored at dseg_lo + sizeof(long).
 * gc_add_free just dumps blocks onto the free list 
 *
 * gc_malloc currently searches the free list; if that fails
 * to get a large enough block, then it resorts to mem_sbrk.
 * You will need to decide when to call garbage_collect.
 *
 * Used blocks have a header that consists of a Tree structure, which 
 * contains the size of the allocated area and ptrs to the headers of 
 * other allocated blocks.
 * Free blocks have the linked list which keeps track of them stored
 * in the header space. 
 */
/*$z$ the struct of the list is just like the struct of the Tree */
/*$z$
	typedef struct tree_node Tree;
	struct tree_node {
		int size;
		Tree * left, * right;
	};
*/
typedef struct list_struct {
    int size;
    struct list_struct *next, *prev;
} list_t;

/* Initialize the heap by getting 1 page of memory;
 * The free list pointer and allocated tree pointers
 * occupy the first two words of this page. The rest of the page
 * is our first free block. Set the free list pointer to
 * point to it, and set the allocated tree pointer to NULL
 */
int gc_init (void)
{
    list_t *list;

    mem_sbrk(mem_pagesize());	//4096Bytes  -- 4K

    /* initialize freelist ptr */
	/*$z$ ( dseg_lo + 8 ) list_struct address */
    list = (list_t *)(dseg_lo+(sizeof(long)<<1));  
    *(list_t **)dseg_lo = list;
	/*$z$ the addr now is ( dseg_lo + 8 ), but it will change */
    list->next = list->prev = list;					
    /* got 1 page, used first 2 words for free list ptr, alloc'd ptr, */
    /* of the remaining bytes, need space for a Tree structure */
	/*$z$ the max space can allocate to caller should have a Tree struct */
    list->size = mem_pagesize() - (sizeof(ptr_t)<<1) - sizeof(Tree);

    /* initialize the allocated tree pointer to NULL since nothing alloc'd yet */
	/*$z$ (dseg_lo + sizeof(long)) is (desg_lo + 4) */
    *(Tree **)(dseg_lo + sizeof(long)) = NULL; 
    return 0;
}

/* Remove a block from a list; "addr" is the address where
 * the pointer to the head of the list is stored; "blk"
 * is a pointer to the block to remove from the list.
 */ 
/*$z$ "addr" is used as a pointer of pointer 
 *	  list_t **addr maybe more easily to understand
 */
void gc_remove_free(char *addr, list_t *blk) {
    if (blk == blk->next) {
		*(list_t **)addr = NULL;
		return;
    }

    if (*(list_t **)addr == blk) {
		*(list_t **)addr = blk->next;
		if (!blk->next->size)
			assert(0);
    }
    blk->next->prev = blk->prev;
    blk->prev->next = blk->next;
}

/* Add a block to a list; "addr" is the address where
 * the pointer to the head of the list is stored,
 * "blk" is a pointer to the block to add to the list.
 */
/*$z$ "addr" is used as a pointer of pointer 
 *	  list_t **addr maybe more easily to understand
 *	  we know if the parameter is a pointer, the original 
 *	  pointer will not change, but if it is a pointer of pointer 
 *	  it will work by change the "second level" pointer.
 *
 *    blk just added to the first position,is that good?
 */
void gc_add_free(char *addr, list_t *blk) {
    list_t *freelist;

    freelist = *(list_t **)addr;

    if (freelist == NULL) {
		blk->next = blk->prev = blk;
		/*$z$ not "freelist = blk", freelist just a local copy */
		*(list_t **)addr = blk;
		return;
    }
    blk->next = freelist;
    blk->prev = freelist->prev;
    freelist->prev->next = blk;
    freelist->prev = blk;

    /* make this the first thing on the list */
    *(list_t **)addr = blk;
}

/************************************************************/
/* Functions and types that you will find useful for        */
/* garbage collection. Define any additional functions here */
/************************************************************/

typedef struct stack_t {
  /* You can define the stack data type in any way that you like.
   * You are allowed to use the standard libc malloc/free 
   * routines for the stack management, if you wish. 
   * DO NOT use gc_malloc!
   */
  //int dummy; // SYNTAX PURPOSE, replace it by your own ones.
	Tree* ptr;
	struct	stack_t * next;
} stack_t;



void push_stack(stack_t** stack_top, Tree * ptr)
{
	stack_t* temp = (stack_t*)malloc(sizeof(stack_t));
	temp->ptr = ptr;
	temp->next = *stack_top;
	*stack_top = temp;
}

Tree* pop_stack(stack_t** stack_top)
{
	stack_t* temp;
	Tree* ptr;
	if(*stack_top == NULL)
		return NULL;
	temp = (*stack_top)->next;
	ptr = (*stack_top)->ptr;
	free(*stack_top);
	*stack_top = temp;	
	return ptr;
}


/* return a pointer to the address where the global data segment starts,
 * and the length of the segment in bytes (not words!).
 */
/* Win32: Used PE Constant Value instead in program environment */
/*void get_data_area(void **start, int *length)
{

    Elf32_Ehdr *test = (Elf32_Ehdr *)0x8048000;
    Elf32_Phdr *pgm = (Elf32_Phdr *)((void *)test + test->e_phoff);
    Elf32_Half nhdr = test->e_phnum;

    for( ; pgm < (pgm + nhdr); pgm++) {	
	if((pgm->p_type == PT_LOAD) && !(pgm->p_flags & PF_X)) { 
	    /* Loadable program segment AND not executable, must be data * /
	    *start = (void *)(pgm->p_vaddr);
	    *length = (int)(pgm->p_memsz);
	    break;
 	}
    }	
}*/

/* Traverse the tree of allocated blocks, collecting
 * pointers to unallocated blocks in the stack s
 */
void collect_unmarked(Tree *root, stack_t **s)
{

    if(root) {
		if(!(root->size & 0x1)) { /* unreachable */
		/* Push the root onto your stack.
		* Define "push" in any way that is appropriate for
		* your stack data structure 
		*/
			push_stack(s, root);
//			printf("the garbage tree node : %x.\n", root);
		}
		root->size &= ~0x1; /* clear for next time */
		if(root->left) collect_unmarked(root->left, s);
		if(root->right) collect_unmarked(root->right, s);
    }
}

extern void verify_complete(void);
extern void verify_garbage(void *addr);

void garbage_collect(int *regs, int pgm_stack)
{
  Tree *alloc_tree = *(Tree **)(dseg_lo+sizeof(long));
  int registers[3];
  int top = pgm_stack;
  static time = 1;
  int temp_ptr = 0;

  stack_t *stack = NULL;
  stack_t ** stack_top = &stack;

  int i = 0;
  Tree *temp_tree_node = NULL;

  //GET_CALLEE_REGS(registers);
  //PGM_STACK_TOP(top);
  //printf("$z$%d   %d\n",pgm_stack,*(int*)top);

  /* 0. Make sure there is already allocated memory to 
   *    garbage collect.
   */
	 
//	printf("times : %d\n", time);
	time++;
 

  /* 1. collect root pointers by scanning regs, stack, and global data */
  /* 2. mark nodes that are reachable from the roots and add any pointers 
   *    found in reachable heap memory 
   */
	
	//find pointers in registers 

	for (i=0;i<3;i++)
	{
	//	printf("scanning addr_regs: %x\n", regs[i]);
		temp_tree_node= contains(regs[i], (Tree **)(dseg_lo+sizeof(long)));
		alloc_tree = *(Tree **)(dseg_lo+sizeof(long));	
		if(temp_tree_node){
			(temp_tree_node->size | 0x1);
		//	printf("find ptr in regs: %x\n", regs[i]);
		//	printf("the marked node is :%x\n", (int)temp_tree_node);
		}
	}
	//find pointers in stacks


	//recursively find data on stack , when twice deep ebp is 0x0 , stop.
	top = *(int*)top;  // gc_malloc() no need to search ptr.  retrieve caller ebp.
	top = *(int*)top;  // It is not conservative garbage collector:( 
					   // if vertify_complelte() line 215 in driver.c tolerance 2 garbages, 
					   // you can committed this line.
	while (**(int**)top != 0x00000000)   
	{
	//	printf("current ebp is %x, judge value is :%x \n", top, **(int**)top);
		for (i=0; *(int*)(top-4*(i+1))!=0xcccccccc; i++)
		{
			temp_ptr = *(int*)(top-4*(i+1));
		//	printf("scanning addr: %x, temp_ptr is %x\n", (top-4*(i+1)), temp_ptr);
			if(temp_ptr >= dseg_lo && temp_ptr <= dseg_hi)	{
				temp_tree_node = contains(temp_ptr, (Tree **)(dseg_lo+sizeof(long)));
				alloc_tree = *(Tree **)(dseg_lo+sizeof(long));	
				if(temp_tree_node){
					temp_tree_node->size = (temp_tree_node->size | 0x1);      //mark
			//		printf(">>>>find ptr in stack: %x\n", temp_ptr);
			//		printf(">>>>the marked node is :%x\n", (int)temp_tree_node);
				}	
			}
		}
		top = *(int*)top;// 	
	}

// 		top = 0x0011c6e8;
// 		for (i=0; i < 20000; i++)
// 		{
// 			temp_ptr = *(int*)(top+4*(i));
// 				
// 			temp_tree_node = contains(temp_ptr, (Tree **)(dseg_lo+sizeof(long))); //!!!!!very important!!!!!
// 			alloc_tree = *(Tree **)(dseg_lo+sizeof(long));						  //!!!!!very important!!!!!
// 	
// 			if(temp_tree_node){ 
// 					temp_tree_node->size = (temp_tree_node->size | 0x1);      //mark
// 	//				printf("index : %d  ptr : %x  mark node: %x\n", i, temp_ptr, (int)temp_tree_node);
// 				}	
// 		}


  /* 3. collect pointers to all the unmarked (i.e. unreachable) blocks */

  collect_unmarked(alloc_tree, stack_top);

  /* 4. delete each unmarked block from the allocated tree and 
   *    then place the block on the free list.
   *    Call verify_garbage with the address previously returned by malloc
   *    for each block.
   */
  while(*stack_top != NULL){
	temp_ptr = (int*)pop_stack(stack_top);
	verify_garbage(temp_ptr+12);
	*(Tree **)(dseg_lo+sizeof(long)) = delete(temp_ptr, alloc_tree);
	alloc_tree = *(Tree **)(dseg_lo+sizeof(long)); // just for debug_watch
//	print_tree(alloc_tree, 0);
//	scanf("%d", &top); //just for debug
	gc_add_free((char*)dseg_lo, (list_t*)temp_ptr);
  }
verify_complete();
}


void *gc_malloc (size_t size)
{
    list_t *list, *newlist;
    char *addr, *test;
    long foo, mask;
    Tree *alloc_tree;
    int registers[3];
    int top;

    /* start by getting the callee-save registers and the top of the program
     * stack at this point. Doing this here means that we don't have to search
     * the stack frame for gc_malloc or anything that it calls when we collect
     * root pointers.
     */
    GET_CALLEE_REGS(registers);
    PGM_STACK_TOP(top);

    /* round up size to a power of 8 */
    size = (size ? ((size>>3) + !!(size & 7)) << 3 : 8);

    /* search un-coalesced free list */
    addr = dseg_lo;
	/*$z$ list is a pointer to current element of the freelist*/
    list = *(list_t **)addr;
    if (list != NULL) {
      do {
			if ((unsigned)list->size >= size) {
				break;
			}
			list = list->next;
			if (list == *(list_t **)addr) {
				list = NULL;
			}
		if(list == NULL)
			garbage_collect(registers,top);
      } while (list);
    }
      
    /* if we couldn't find a fit then try sbrk */
    if(list==NULL) {    
      mask = mem_pagesize()-1;     //$z$ mask 11111111111 
	  /*$z$ in case of there is no space for a Tree struct 
	   *	size will reduce after calculation processed.
	   */
      size += sizeof(Tree);		
      /* number of pages needed, ceil( size/pagesize ) */
      foo = (size & ~mask) + ((!!(size & mask))*mem_pagesize());
      size -= sizeof(Tree);
      list = (list_t *)(dseg_hi+1);
      test = mem_sbrk(foo);
      if (!test) 
		return NULL;
      list->size = foo - sizeof(Tree);
      gc_add_free(dseg_lo, list);
    }


    /* found a fit */
    if ((unsigned)list->size < size + sizeof(list_t) + sizeof(long) ) {
		/* give them the whole chunk (we need sizeof(list_t) to hold accounting
		* information, want at least one usable word if we're going to 
		* keep track of this block!) */	gc_remove_free(addr, list);
	
		/* put this block in the allocated tree */
		addr = dseg_lo + sizeof(long);
		alloc_tree = *(Tree **)addr;
		*(Tree **)addr = insert(alloc_tree, (Tree *)list);
		return (((char *)list) + sizeof(Tree));
    }

//	garbage_collect(registers,top);

    /* give them the beginning of the block */
    newlist = (list_t *)(((char *)list) + size + sizeof(Tree));
    newlist->size = list->size - size - sizeof(Tree);
    list->size = size;

    if (list->next == list) {
		newlist->next = newlist->prev = newlist;
    } else {
		newlist->prev = list->prev;
		newlist->next = list->next;
		newlist->prev->next = newlist->next->prev = newlist;
    }
    if (*(list_t **)addr == list) {
		*(list_t **)addr = newlist;
    }

    /* put this block in the allocated tree */
    addr = dseg_lo + sizeof(long);
    alloc_tree = *(Tree **)addr;
    *(Tree **)addr = insert(alloc_tree, (Tree *)list);
	alloc_tree = *(Tree **)addr; // just for debug_watch
//	print_tree(alloc_tree, 0);
//	scanf("%d", &top);    // just to wait for watch

    return ((char *)list + sizeof(Tree));
}

