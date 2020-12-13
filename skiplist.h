/*
 * Interface for the skip list data structure.
 *
 * Author: Henry Daly, 2017
 */
#ifndef SKIPLIST_H_
#define SKIPLIST_H_

#include <atomic_ops.h>
#include <stdio.h>
#include <stdlib.h>
#include <numa.h>
#include <numaif.h>
#include "allocator.h"
#include "common.h"


/* define for search layer and nohotspot address checking
 * 	this is a sanity check to ensure that all memory addresses accessed
 * 	are in the NUMA zone in which they should reside and for tracking
 * 	percent local accesses in application thread execution
 */
//#define ADDRESS_CHECKING
numa_allocator** allocators;
#define MAX_LEVELS 128

#define NUM_LEVELS 2
#define NODE_LEVEL 0
#define INODE_LEVEL 1

typedef unsigned long sl_key_t;
typedef void* val_t;
typedef unsigned int uint;


/* data layer nodes */
typedef VOLATILE struct sl_node node_t;
struct sl_node {
	struct sl_node*		prev;
	struct sl_node*		next;
	val_t 				val;
	sl_key_t 			key;
	volatile uint		level;
	bool 				fresh;
};

/* index layer nodes */
typedef VOLATILE struct sl_inode inode_t;
struct sl_inode {
	struct sl_inode*	right;
	struct sl_inode*	down;
	struct sl_mnode*	intermed;
	sl_key_t 		 	key;
};

/* intermediate layer nodes */
typedef VOLATILE struct sl_mnode mnode_t;
struct sl_mnode {
	struct sl_mnode* 	next;
	struct sl_node*		node;
	sl_key_t			key;
	unsigned int		level;
	bool				marked;
};

node_t* node_new(sl_key_t key, val_t val, node_t *prev, node_t *next);
inode_t* inode_new(inode_t *right, inode_t *down, mnode_t* intermed, int zone);
mnode_t* mnode_new(mnode_t* next, node_t* node, unsigned int level, int zone);

void node_delete(node_t *node);
void inode_delete(inode_t *inode, int zone);
void mnode_delete(mnode_t* mnode, int zone);
int data_layer_size(node_t* head, int flag);

#define NODE_SZ 	sizeof(node_t)
#define INODE_SZ	sizeof(inode_t)
#define MNODE_SZ	sizeof(mnode_t)

/* initial_populate serves as a manner in which initial population of the
 * index layer (which will be scrapped at end of populate) will not waste
 * allocator space
 */
extern bool	initial_populate;

/* - Public skiplist interface - */

/**
 * node_new() - create a new data layer node
 * @key  - the key for the new node
 * @val  - the val for the new node
 * @prev - the prev node pointer for the new node
 * @next - the next node pointer for the new node
 */
node_t* node_new(sl_key_t key, val_t val, node_t *prev, node_t *next)
{
	node_t *node;
	node = (node_t*)malloc(NODE_SZ);
	node->key       = key;
	node->val       = val;
	node->prev      = prev;
	node->next      = next;
	node->fresh		= true;
	node->level		= 0;
	return node;
}

/**
 * inode_new() - create a new index node
 * @right	 - the right inode pointer for the new inode
 * @down	 - the down inode pointer for the new inode
 * @node	 - the node pointer for the new inode
 * @intermed - intermediate layer node
 * @zone	 - NUMA zone
 */
inode_t* inode_new(inode_t *right, inode_t *down, mnode_t* intermed, int zone)
{
	inode_t *inode;
	if(initial_populate) {
		inode = (inode_t*)malloc(INODE_SZ);
		inode->right 	= right;
		inode->down 	= down;
		inode->intermed = intermed;
		inode->key 		= intermed->key;
		return inode;
	}

	numa_allocator* local = allocators[zone];
	inode = (inode_t*)local->nalloc(INODE_SZ);
	inode->right 	= right;
	inode->down 	= down;
	inode->intermed = intermed;
	inode->key 		= intermed->key;
	return inode;
}

/**
 * mnode_new() - create a new intermediate node
 * @next  - the right mnode pointer for the new mnode
 * @node  - the data layer node for the the new mnode
 * @level - the starting level of the new mnode
 * @zone  - NUMA zone
 */
mnode_t* mnode_new(mnode_t* next, node_t* node, unsigned int level, int zone) {

	mnode_t* mnode;
	numa_allocator* local = allocators[zone];
	mnode = (mnode_t*)local->nalloc(MNODE_SZ);
	mnode->level 	= level;
	mnode->key 		= node->key;
	mnode->next 	= next;
	mnode->marked 	= false;
	mnode->node 	= node;
	return mnode;
}

/**
 * node_delete() - delete a data layer node
 * @node - the node to delete
 */
void node_delete(node_t *node)
{
	free((void*)node);
}

/**
 * inode_delete() - delete an index layer node
 * @inode - the index node to delete
 * @zone  - NUMA zone
 */
void inode_delete(inode_t *inode, int zone)
{
	numa_allocator* local = allocators[zone];
	local->nfree(inode, INODE_SZ);
}

/**
 * mnode_delete() - delete an intermediate node
 * @mnode - the intermediate node to delete
 * @zone  - NUMA zone
 */
void mnode_delete(mnode_t* mnode, int zone) {
	numa_allocator* local = allocators[zone];
	local->nfree(mnode, MNODE_SZ);
}

/**
 * data_layer_size() - returns the size of the data layer
 * @head - the sentinel node for the data layer
 * @flag - specifies if we include logically deleted nodes
 *
 */
int data_layer_size(node_t* head, int flag)
{
	struct sl_node *node = head;
	int size = 0;

	node = node->next;
	while (NULL != node) {
		if (flag && (NULL != node->val && node != node->val))
			++size;
		else if (!flag && node->key != 0)
			++size;
		node = node->next;
	}

	return size;
}


#ifdef ADDRESS_CHECKING
	int check_addr(int supposed_node, void* addr);
	void zone_access_check(int supposed_node, void* addr, volatile long* local, volatile long* foreign, bool dont_count);
#endif

#endif /* SKIPLIST_H_ */
