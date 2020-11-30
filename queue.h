/*
 * Interface for SPSC Queue
 *
 * Author: Henry Daly, 2018
 */

#ifndef QUEUE_H_
#define QUEUE_H_

#include "skiplist.h"
#include "common.h"
#include <numa.h>
#include <stdio.h>

struct q_node {
	q_node*	next;
	node_t*	node;
};

class update_queue{
private:
	q_node* head;
	q_node* tail;
	q_node* stub;

public:
			update_queue();
			~update_queue();
	void	push(node_t* ptr);
	q_node*	pop();
};

/* new_qnode() - allocates an empty qnode */
q_node* new_qnode(node_t* ptr) {
    q_node* new_job = (q_node*)malloc(sizeof(q_node));
    new_job->node = ptr;
    new_job->next = 0;
    return new_job;
}
/* Constructor */
update_queue::update_queue()
{
    stub = new_qnode(0);
    head = tail = stub;
}

/* Destructor */
update_queue::~update_queue()
{
    q_node* f = head;
    while(f) {
        q_node* x = f->next;
        free(f);
        f = x;
    }
}

/* push() - pushes node to queue */
void update_queue::push(node_t* ptr) {
    q_node* spot = new_qnode(ptr);
    tail->next = spot;
    tail = spot;

}

/* pop() - attempts to pop node from queue */
q_node* update_queue::pop(void) {
    q_node* popped = head;
    if(!head->next) return NULL;
    head = head->next;
    popped->node = head->node;
    return popped;
}

#endif /* QUEUE_H_ */
