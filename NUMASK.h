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
#include <numa.h>
#include <atomic_ops.h>
#include "common.h"
#include "tm.h"
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

template<class RecordManager, typename sl_key_t, typename val_t>
class SkipListNUMA {
private:

    RecordManager * const recmgr;
    const int numThreads;
    const int minKey;
    const long long maxKey;
    static size_t maxNUMA;
    node_t* head = node_new(0, NULL, NULL, NULL);
    search_layer* sl[maxNUMA];

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
        maxNUMA = numa_max_node();
        mnode_t* mnode = mnode_new(NULL, head, 1, tid);
        inode_t* inode = inode_new(NULL, NULL, mnode, tid);
        for (int i=0; i<maxNUMA; i++) {
            sl[i] = new search_layer(tid, inode, new update_queue());
        }

        for (int i=0; i<maxNUMA; i++){
            sl[i]->start_helper(0);
        }
    }

    ~SkipListNUMA(){
        for (int i=0; i<maxNUMA; i++){
            sl[i]->stop_helper();
        }
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

    void initThread(const int tid);

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


