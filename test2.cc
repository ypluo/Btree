

#include <iostream>
#include <fstream>
#include <vector>
#include <cstring>
#include <thread>
#include <random>
#include <algorithm>

#include "btree.h"
#include "btree_unsort.h"
#include "slotonly.h"

#define DEBUG true

using std::cout;
using std::endl;
using mykey_t = int64_t;
using myvalue_t = int64_t;

mykey_t * keys;
int * insert_order;

template <typename BTreeType>
void put_throughput(BTreeType &tree, uint32_t scale, uint32_t req_cnt, uint32_t thread_id) {
    thread_local std::default_random_engine rd(thread_id);
    thread_local std::uniform_int_distribution<uint32_t> dist(0, scale);
    
    int offset = thread_id * req_cnt;
    for(int i = 1; i <= req_cnt; i += 1) {
        mykey_t key = keys[insert_order[offset + i]];
        tree.insert((mykey_t)key, (myvalue_t)key);
    }

    cout << thread_id << " finish insert " << endl;
}

template <typename BTreeType>
void get_throughput(BTreeType &tree, uint32_t scale, uint32_t req_cnt, uint32_t thread_id) {
    thread_local std::default_random_engine rd(thread_id);
    thread_local std::uniform_int_distribution<uint32_t> dist(0, scale);
    int64_t val;
    uint32_t notfound = 0;

    for(int i = 1; i <= req_cnt; i++) {
        mykey_t key = keys[dist(rd)];
        if(!tree.find(key, val)) {
            notfound++;
        } 
    }

    cout << thread_id << " finish get " << notfound << endl;
}

template <typename BTreeType>
void del_throughput(BTreeType &tree, uint32_t scale, uint32_t req_cnt, uint32_t thread_id) {
    thread_local std::default_random_engine rd(thread_id);
    thread_local std::uniform_int_distribution<uint32_t> dist(0, scale);
    
    for(int i = 0; i < req_cnt; i++) {
        mykey_t key = keys[dist(rd)];
        tree.remove(key);
        //cout << thread_id << " " << key << endl;
    }
    cout << thread_id << " finish delete " << endl;
}

template <typename BTreeType>
void update_throughput(BTreeType &tree, uint32_t scale, uint32_t req_cnt, uint32_t thread_id) {
    thread_local std::default_random_engine rd(thread_id);
    thread_local std::uniform_int_distribution<uint32_t> dist(0, scale);

    for(int i = 1; i <= req_cnt; i++) {
        mykey_t key = keys[dist(rd)]; 
        tree.update(key, key * 2);
        //cout << thread_id << " " << key << endl;
    }

    cout << " finish update " << endl;
}

template <typename BTreeType>
void exp1(BTreeType &tree, uint32_t scale, uint32_t req_cnt, uint32_t thread_id) {
    put_throughput(tree, scale, req_cnt, thread_id);

    //get_throughput(tree, scale, req_cnt, thread_id);

    //update_throughput(tree, scale, req_cnt, thread_id);

    del_throughput(tree, scale, req_cnt, thread_id);
}


int main(int argc, char ** argv) {
    uint32_t scale = 10000;
    uint32_t thread_cnt = 1;
    int test_id = 1;

    if (argc > 1) scale = atoi(argv[1]);
    if(argc > 2) thread_cnt = atoi(argv[2]);
    if(argc > 3) test_id = atoi(argv[3]);

    #ifdef DEBUG
        cout << "SCALE:" << scale << endl;
        cout << "Threads: " << thread_cnt << endl;
        cout << "Test load type: " << test_id << endl;
    #endif
    
    keys = new mykey_t[scale];
    insert_order = new int[scale];
    uint32_t steps = 100;
    for(int i = 0; i < scale; i++) {
        keys[i] = i * steps;
        insert_order[i] = i;
    }
    std::shuffle(insert_order, insert_order + scale - 1, std::default_random_engine(99));

    tree_api * tree;

    auto start = seconds();
    
    std::vector<std::thread> threads;
    for(int i = 0; i < thread_cnt; i++) {
        switch(test_id){
        case 1:
            threads.push_back(std::thread(put_throughput<decltype(tree)>, std::ref(tree), scale, scale / thread_cnt, i));
            break;
        case 2:
            threads.push_back(std::thread(get_throughput<decltype(tree)>, std::ref(tree), scale, scale / thread_cnt, i));
            break;
        case 3:
            threads.push_back(std::thread(update_throughput<decltype(tree)>, std::ref(tree), scale, scale / thread_cnt, i));
            break;
        case 4:
            threads.push_back(std::thread(del_throughput<decltype(tree)>, std::ref(tree), scale, scale / thread_cnt, i));
            break;
        case 5:
            threads.push_back(std::thread(exp1<decltype(tree)>, std::ref(tree), scale, scale / thread_cnt, i));
            break;
        default:
            cout << "Not a valid test load type (1-4)" << endl;
            return 0;
        }
    }

    for(int i = 0; i < thread_cnt; i++) {
        threads[i].join();
    } // waiting for all the thread to stop

    auto end = seconds();

    cout << "Time Elapse: " << end - start << endl;

    delete keys;
    delete insert_order;

    return 0;
}
