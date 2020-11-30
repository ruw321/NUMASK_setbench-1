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
    node_t* head;
    search_layer* sl;

    node_t* createNode(const int tid, sl_key_t key, val_t value, node_t *prev, node_t *next){
        node_t *node;
        node = recmgr->template allocate<node_t>(tid);
        node->key       = key;
        node->val       = value;
        node->prev      = prev;
        node->next      = next;
        node->fresh		= true;
        node->level		= 0;
        return node;
    };

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

    SkipListNUMA(const int _numThreads, const int _minKey, const long long _maxKey);

    ~SkipListNUMA();

    bool contains(const int tid, const sl_key_t &key){
        return sl_contains_old(sl, key, TRANSACTIONAL);
    };

    bool insertIfAbsent(const int tid, const sl_key_t &key, const val_t &value){
        return sl_add_old(sl, key, value, TRANSACTIONAL);
    };

    bool erase(const int tid, const sl_key_t &key){
        return sl_remove_old(sl, key, TRANSACTIONAL);
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
SkipListNUMA<RecordManager, sl_key_t , val_t>::SkipListNUMA(const int _numThreads, const int _minKey, const long long _maxKey)
        : numThreads(_numThreads), minKey(_minKey), maxKey(_maxKey), recmgr(new RecordManager(numThreads)) {
    head = createNode(tid, 0, NULL, NULL, NULL);
}

template<class RecordManager, typename sl_key_t , typename val_t>
SkipListNUMA<RecordManager, sl_key_t , val_t>::~SkipListNUMA() {
}

template<class RecordManager, typename sl_key_t , typename val_t>
void SkipListNUMA<RecordManager, sl_key_t , val_t>::initThread(const int tid) {
    recmgr->initThread(tid);
}

template<class RecordManager, typename sl_key_t , typename val_t>
void SkipListNUMA<RecordManager, sl_key_t , val_t>::deinitThread(const int tid) {
    recmgr->deinitThread(tid);
}


