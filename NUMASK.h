//
// Created by Anlan Yu on 11/26/20.
//
#pragma once

#include <cassert>


#define MAX_KCAS 41

#include <unordered_set>
#include <iostream>
#include <fstream>
#include <iomanip>
#include <ctime>
#include <immintrin.h>
#include <assert.h>
#include <getopt.h>
#include <limits.h>
#include <pthread.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/time.h>
#include <time.h>
#include <stdint.h>
#include <unistd.h>
///#include <numa.h>
#include "../common/numa_tools.h"
#include <atomic_ops.h>
#include "common.h"
//#include "tm.h"
#include "skiplist.h"
#include "queue.h"
#include "search_and_bg.h"
#include "nohotspot_ops.h"

using namespace std;

#define MAX_THREADS 200
#define MAX_PATH_SIZE 64
#define PADDING_BYTES 128
#define MAX_TOWER_HEIGHT 20

#define IS_MARKED(word) (word & 0x1)

#define MAX_LEVELS 128

#define NUM_LEVELS 2
#define NODE_LEVEL 0
#define INODE_LEVEL 1
#define TRANSACTIONAL 4
#define MAX_NUMA_ZONES                  numa_max_node() + 1
#define MIN_NUMA_ZONES					1

template<class RecordManager, typename sl_key_t, typename val_t>
class SkipListNUMA {
private:

    RecordManager * const recmgr;
    const int numThreads;
    const int minKey;
    const long long maxKey;
    static int maxNUMA;
    node_t* head = node_new(0, NULL, NULL, NULL);
    //search_layer* sl[maxNUMA];
    search_layer** search_layers;
    extern numa_allocator** allocators;
    struct zone_init_args {
        int 		numa_zone;
        node_t* 	node_sentinel;
        unsigned	allocator_size;
    };
    bool numask_complete;
    pthread_t dhelper_thread;     //data layer helper thread (only one)

    /* zone_init() - initializes the queue and search layer object for a NUMA zone	*/
    void* zone_init(void* args) {
        zone_init_args* zia = (zone_init_args*)args;

        cpu_set_t cpuset;
        CPU_ZERO(&cpuset);
        CPU_SET(zia->numa_zone, &cpuset);
        sleep(1);
        pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
        numa_set_preferred(zia->numa_zone);

        numa_allocator* na = new numa_allocator(zia->allocator_size);
        allocators[zia->numa_zone] = na;

        mnode_t* mnode = mnode_new(NULL, zia->node_sentinel, 1, zia->numa_zone);
        inode_t* inode = inode_new(NULL, NULL, mnode, zia->numa_zone);
        search_layer* sl = new search_layer(zia->numa_zone, inode, new update_queue());
        search_layers[zia->numa_zone] = sl;
        free(zia);
        return NULL;
    }

    /* private functions */
    int sl_contains_old(search_layer *sl, unsigned int key, int transactional)
    {
        return sl_contains(sl, (sl_key_t) key);
    }

    int sl_add_old(search_layer *sl, unsigned int key, unsigned int val, int transactional)
    {
        return sl_insert(sl, (sl_key_t) key, (val_t) val);
    }

    int sl_remove_old(search_layer *sl, unsigned int key, int transactional)
    {
        return sl_delete(sl, (sl_key_t) key);
    }

public:

    SkipListNUMA(const int _numThreads, const int _minKey, const long long _maxKey) {
        maxNUMA = MAX_NUMA_ZONES;
        mnode_t* mnode = mnode_new(NULL, head, 1, tid);
        inode_t* inode = inode_new(NULL, NULL, mnode, tid);

        // create sentinel node on NUMA zone 0
        node_t* sentinel_node = node_new(0, NULL, NULL, NULL);

        // create search layers
        search_layers = (search_layer**)malloc(num_numa_zones*sizeof(search_layer*));
        pthread_t* thds = (pthread_t*)malloc(num_numa_zones*sizeof(pthread_t));
        allocators = (numa_allocator**)malloc(num_numa_zones*sizeof(numa_allocator*));

        for(int i = 0; i < num_numa_zones; ++i) {
            zone_init_args* zia = (zone_init_args*)malloc(sizeof(zone_init_args));
            zia->numa_zone = i;
            zia->node_sentinel = sentinel_node;
            zia->allocator_size = buffer_size;
            pthread_create(&thds[i], NULL, zone_init, (void*)zia);
	    }

        for(int i = 0; i < num_numa_zones; ++i) {
            void *retval;
            pthread_join(thds[i], &retval);
        }

        // start data-layer-helper thread
        numask_complete = false;
        bg_dl_args* data_info = (bg_dl_args*)malloc(sizeof(bg_dl_args));
        data_info->head = sentinel_node;
        data_info->tsleep = 15000;
        data_info->done = &numask_complete;
        pthread_create(&dhelper_thread, NULL, data_layer_helper, (void*)data_info);

        // start the per-NUMA-helper thread 
        for(int i = 0; i < num_numa_zones; ++i) {
            search_layers[i]->start_helper(0);
        }

        // TODO: nullify all the index nodes we created so
        // we can start again and rebalance the skip list
        // wait until the list is balanced

        // TODO: add a barrier so that we wait for every thread to be up and running 
    }

    ~SkipListNUMA(){
        // stop background threads
	    numask_complete = true;
        // stop the helper threads
        for(int i = 0; i < num_numa_zones; ++i) {
            search_layers[i]->stop_helper();
        }
        // join the data layer helper thread
        pthread_join(dhelper_thread, NULL);
    };

    bool contains(const int tid, const sl_key_t &key){
        int numaIndex = numa_node_of_cpu(tid);
        return sl_contains_old(sl[numaIndex], key, TRANSACTIONAL);
    };

    bool insertIfAbsent(const int tid, const sl_key_t &key, const val_t &value){
        int numaIndex = numa_node_of_cpu(tid);
        return sl_add_old(sl[numaIndex], key, value, TRANSACTIONAL);
    };

    bool erase(const int tid, const sl_key_t &key){
        int numaIndex = numa_node_of_cpu(tid);
        return sl_remove_old(sl[numaIndex], key, TRANSACTIONAL);
    };

//    bool validate();
//
//    void printDebuggingDetails();

    void initThread(const int tid) {
        // TODO: get the search layer from the thread 
        // get the cur_zone (get_zone) from the search layer
        // numa_run_on_node(cur_zone)
    };

    void deinitThread(const int tid);


    RecordManager * const debugGetRecMgr() {
        return recmgr;
    }

};
template<class RecordManager, typename sl_key_t , typename val_t>
void SkipListNUMA<RecordManager, sl_key_t , val_t>::initThread(const int tid) {
    recmgr->initThread(tid);
}

template<class RecordManager, typename sl_key_t , typename val_t>
void SkipListNUMA<RecordManager, sl_key_t , val_t>::deinitThread(const int tid) {
    recmgr->deinitThread(tid);
}


