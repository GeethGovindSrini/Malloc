/*
-	This program currently uses a 16bit alignment for malloc.
-	It can be made efficient by aligning to 12bits.	
*/
#include<stdio.h>
#include<unistd.h>
#include<string.h>

struct block{
	size_t size;				//size of the block.
	int free;				//is the block free ot not.
	struct block * next;			//address of the next block.
	struct block * prev;			//address of the prev block.
	void * ptr;				//User Address(Address to be returned to the user).
	char data[1];				//end of the book keeping block.
};
typedef struct block * block;
#define BLOCK_SIZE (int)(sizeof(size_t)+sizeof(int)+3*sizeof(void *))		//defining the size of the block.
//#define BLOCK_SIZE 36		//defining the size of the block.
block base= NULL;		//heap header (initially NULL).
void msg(char *p){
	write(1,p,strlen(p));
}
//------------------------IMPLEMENTATION OF MALLOC-----------------------------//
void printheapstatus(){				//printing the current status of heap.
	block head= base;
	printf("\t\t\t\t-----------------------HEAP STATUS---------------------\n");
	while(head){
		printf("\tb->size: %ld\t", head->size);
		printf("\tb->free: %d\t", head->free);
		printf("\tb->next: %p\t", head->next);
		printf("\tb->prev: %p\t", head->prev);
		printf("\tb->ptr: %p\n", head->ptr);
		head= head->next;
	}
}
size_t align16(size_t n)				//aligning to 16bits.
{
  if(n <= 16)
    return(16);
  if(n%16 == 0)
    return(n);
  return((n/16+1)*16);
}
block extend_heap(block last, size_t s){	//increase the program break by the size given.
	block b;
	b= sbrk (0);
	if(sbrk(BLOCK_SIZE+s)< (void *) 0)		//if sbrk fails.
		return (NULL);
	if(last){				//if some chunks of memory have already been allocated.
		last->next = b;			//attach the newly created one at the end of the existing list of allocated memory.
	}
	b->size= s;				//the size increased is the size of the new block.
	b->next= NULL;				//this chunk of memory is the last one.
	b->ptr= b->data;			//setting ptr to point to the 
	b->prev= last;				//setting address of previous chunk
	b->free = 0;				//set the free flag that it is not free.
	return (b);				//return the created chunk of memory.
}
void split_block(block b,size_t s){		//split the existing chunk of memory to the size asked.
	block new;
	new=(struct block *) b->ptr + s;	//the new's address will be s bytes after b's user address.
	size_t i= b->size;
	new->size= (b->size)-s-BLOCK_SIZE;	//new's size will be got by removing s and the block size from the old user's address(b's size).
	new->next= b->next;			//setting new's next.
	new->prev= b;				//setting next's prev.
	new->free= 1;				//this chunk of memory is free.
	new->ptr= new->data;			//setting ptr to point to the user's piece of memory.	
	b->size= s;				//b's size is s
	b->next= new;				//b's next will be new.
	if(new->next)
		new->next->prev= new;
}
block find_block(block *last, size_t size){	//finding a block that will fit the requested size (using first fit).
	block b= base;				//set b to the base
	while(b && !(b->free && (b->size >= size))){	//if b is not null and b is free and the size of b is greater than requested.
		*last= b;			//at the end last will have the address os the last visited chunk.		
		//printf("value of b->ptr:%p\n", b->ptr);
		b= b->next;			//setting b to the next chunk.
	}
	return b;				//return the fitting chunk or NULL if nothing was found.
}
void * malloc(size_t size){			//implementation of malloc.
	block b,last;
	size_t s;
	s= align16(size);			//size allignment.
	if(base){				//if there exists some allocated chunks.
		last= base;			//setting the last to the base address.
		b= find_block(&last, s);	//finding the first fiiting chunk of memory.
		if(b){					//if we have one fitting chunk.
			if((b->size-s) >= (BLOCK_SIZE+16))	//checking if it can be split.
				split_block(b,s);	//split the block.
			b->free =0;			//set the free flag to not free.
		}else{					//if no block was available to fit the size, we need to extend the heap.
			b = extend_heap(last ,s);	//extending the heap.
			if (!b)				//if extending failed.
				return(NULL);		//return null to the user.
		}
	}else{					//malloc is being called for the first time.
		b= extend_heap(NULL, s);	//extend the heap.
		if (!b)				//if extending failed.
			return (NULL);		//return NULL
		base= b;			//otherwise set the base pointer to the created chunk.
	}
	printheapstatus();
	return(b->ptr);			//return the user's address of the chunk to the user.
}
//--------------------IMPLEMENTATION OF FREE-----------------------------//
block join(block b){
	while(b->next && b->next->free){	//if there are continuous blocks which are free, we can join them.
		//msg("joining\n");
		b->size+= BLOCK_SIZE+b->next->size;	//size of the current block is existing size+size of the next block.
		b->next= b->next->next;		//the next block to the current block is the next block the combined block.
	}
	if(b->next)
		b->next->prev= b;		//if some block exists after b, the it's prev is b.
	return b;				//returning the joint chunk of memory.
}
block get_block(void *p){			//getting the book keeping block when free is called.
	char *tmp= p;				//we assign the address to a char* as we have a ptr[] in our struct block.
	void *temp= NULL;
	block b= NULL;
	tmp-= BLOCK_SIZE+4;			//we go backwards to the starting address of our block.
	temp= tmp;				//we assign the value back to p so that it points to the book keeping block.
	b= (block)temp;
	return(temp);				//return the new address.
}
int isvalidadd(void * p){			//check if the address is valid
	if(base){				//if base exists
		if(p>(void *)base && p<(void *)sbrk(0)){	//the address is greater than base address and less than the current program break.
			if(p== get_block(p)->ptr)	//and the book keeping data matches with the given address
				return 1;	//return 1
		}
	}
	return 0;				//if above stmts fails return 0;
}
void free(void *p){
	block b;
	if(isvalidadd(p)){			//if the entered address is valid
		//msg("valid address\n");
		b= get_block(p);		//get the book keeping block
		b->free= 1;			//set the free flag to 1
		if(b->prev && b->prev->free)	//check if prev block is free
			b= join(b->prev);	//join the blocks
		if(b->next){			//if next block exists
			b= join(b);		//try joining
		}else{				//no next block	
			if(b->prev)		//if there is a prev
				b->prev->next= NULL;	//set it's next to NULL
			else{			//id there are no block in the prev
				base= NULL;	//set the base address to null;
			}
			brk(b);			//set the program break to wherever b is pointing;
		}
	}
	printheapstatus();
}
int main(){
	int *p1, *p2, *p3, *p4, *p5, *p6;
	int *a1, *a2, *a3, *a4, *a5, *a6;
	setvbuf(stdout,NULL,_IONBF,0);		//asking printf to not use malloc.	
	printf("\t\t--------------------------------------BLOCK SIZE:%d--------------------------------------\n", BLOCK_SIZE);
	//printf("pb=%p\n",sbrk(0));
	p1= malloc(1024);
	printf("p1= %p\n",p1);
	a1= p1;
	p2= malloc(32);
	printf("p2= %p\n",p2);
	a2= p2;
	free(p1);
	p3= malloc(16);
	printf("p3= %p\n",p3);
	a3= p3;
	free(p2);
	p4= malloc(16);
	printf("p4= %p\n",p4);
	a4= p4;
	free(p3);
	free(p4);
	/*p5= malloc(77);
	a5= p5;
	p6= malloc(105);
	a6= p6;
	free(p4);
	free(p5);
	p1= malloc(50);
	free(p2);
	free(p3);
	free(p6);
	/*printf("p1= %p\n",a1);
	p1[103415]=2018;
	for(int i= 0;i<110000;i++){
		printf("i=%d\n", i);
		p1[i]=2018;
	}
	free(p1);*/
	/*printf("p1= %p\n",a1);
	printf("p2= %p\n",a2);
	printf("p3= %p\n",a3);
	printf("p4= %p\n",a4);
	printf("p5= %p\n",a5);
	printf("p6= %p\n",a6);
	printf("p1= %p\n",p1);*/
}
