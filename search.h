/*
 * Interface for search layer object
 *
 * Author: Henry Daly, 2017
 */
#ifndef SEARCH_H_
#define SEARCH_H_

#include <pthread.h>
#include "queue.h"
#include "skiplist.h"
#include "background.h"
#include "stdio.h"

// Uncomment to collect background stats - reduces performance
//#define BG_STATS

#ifdef BG_STATS
	typedef struct bg_stats bg_stats_t;
	struct bg_stats {
			int raises;
			int loops;
			int lowers;
			int delete_succeeds;
	};
#endif

class search_layer {
private:
	inode_t* 		sentinel;
	pthread_t 		helper;
	int 			numa_zone;
	update_queue*	updates;
public:
	bool finished;
	bool running;
	int bg_non_deleted;
	int bg_tall_deleted;
	int sleep_time;
	bool repopulate;

	search_layer(int nzone, inode_t* ssentinel, update_queue* q);;
	~search_layer();
	void start_helper(int);
	void stop_helper(void);
	inode_t* get_sentinel(void);
	inode_t* set_sentinel(inode_t*);
	int get_zone(void);
	update_queue* get_queue(void);
	void reset_sentinel(void);

#ifdef ADDRESS_CHECKING
	bool			index_ignore;
	volatile long 	bg_local_accesses;
	volatile long 	bg_foreign_accesses;
	volatile long 	ap_local_accesses;
	volatile long 	ap_foreign_accesses;
#endif
	#ifdef BG_STATS
	bg_stats_t shadow_stats;
	void bg_stats(void);
#endif

};

/* Constructor */
search_layer::search_layer(int nzone, inode_t* ssentinel, update_queue* q)
        :finished(false), running(false), numa_zone(nzone), bg_tall_deleted(0), bg_non_deleted(0),
         repopulate(false), sentinel(ssentinel), updates(q), sleep_time(0), helper(0)
{
    srand(time(NULL));
#ifdef BG_STATS
    shadow_stats.loops = 0;
	shadow_stats.raises = 0;
	shadow_stats.lowers = 0;
	shadow_stats.delete_succeeds = 0;
#endif
#ifdef ADDRESS_CHECKING
    bg_local_accesses = 0;
	bg_foreign_accesses = 0;
	ap_local_accesses = 0;
	ap_foreign_accesses = 0;
	index_ignore = true;
#endif
}

/* Destructor */
search_layer::~search_layer()
{
    if(!finished) {
        stop_helper();
    }
}

/* start_helper() - starts helper thread */
void search_layer::start_helper(int ssleep_time) {
    sleep_time = ssleep_time;
    if(!running) {
        running = true;
        finished = false;
        pthread_create(&helper, NULL, per_NUMA_helper, (void*)this);
    }
}
/* stop_helper() - stops helper thread */
void search_layer::stop_helper(void) {
    if(running) {
        finished = true;
        pthread_join(helper, NULL);
        //BARRIER();
        running = false;
    }
}

/* get_sentinel() - return sentinel index node of search layer */
inode_t* search_layer::get_sentinel(void) {
    return sentinel;
}

/* set_sentinel() - update and return new sentinel node */
inode_t* search_layer::set_sentinel(inode_t* new_sent) {
    return (sentinel = new_sent);
}

/* get_node() - return NUMA zone of search layer */
int search_layer::get_zone(void) {
    return numa_zone;
}

/* get_queue() - return queue of search layer */
update_queue* search_layer::get_queue(void) {
    return updates;
}

/* reset_sentinel() - sets flag to later reset sentinel to fix towers */
void search_layer::reset_sentinel(void) {
    repopulate = true;
}


#endif
