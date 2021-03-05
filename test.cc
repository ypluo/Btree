#include <iostream>
#include <fstream>
#include <cstdint>
#include <cstdlib>
#include <vector>
#include <random>
#include <ctime>

#include "btree.h"
#include "btree_unsort.h"
#include "slotonly.h"
#include "cmdline.h"

using std::cout;
using std::endl;
using std::ifstream;

double put_throughput(tree_api *tree, std::vector<_key_t> keys) {
    auto start = seconds();
    for(int i = 0; i < keys.size(); i += 1) {
        _key_t key = keys[i];
        //cout << key << endl;
        tree->insert((_key_t)key, (_value_t)key);
    } 
    //tree->printAll();

    auto end = seconds();
    return double(end - start);
}

double get_throughput(tree_api *tree, std::vector<_key_t> keys) {
    auto start = seconds();
    _value_t val;
    for(int i = 0; i < keys.size(); i++) {
        _key_t key = keys[i];
        if(!tree->find(key, val)) {
            cout << key << " "<< 0 << endl;
        } 
        //cout << key << " " << val << endl;
    }
    auto end = seconds();
    return double(end - start);
}

double update_throughput(tree_api *tree, std::vector<_key_t> keys) {
    auto start = seconds();
    for(int i = 0; i < keys.size(); i += 1) {
        _key_t key = keys[i];
        tree->update((_key_t)key, (_value_t)key - 1); 
    } 
    auto end = seconds();
    return double(end - start);
}

double del_throughput(tree_api *tree, std::vector<_key_t> keys) {
    auto start = seconds();
    _value_t val;
    for(int i = 0; i < keys.size(); i++) {
        _key_t key = keys[i];
        cout << key << endl;
        // if(key == 594) {
        //     int a =0;
        // }
        tree->remove(key);
        //tree->printAll();
    }
    auto end = seconds();
    return double(end - start);
}


int main(int argc, char ** argv) {
    cmdline::parser pars;
    pars.add<int>("scale", 's', "number of records to insert", false, 100);
    pars.add<int>("tree", 't', "the tree type", true, 1, cmdline::range(1, 4));
    pars.parse_check(argc, argv);

    int test_scale = pars.get<int>("scale");
    int tree_id = pars.get<int>("tree");

    tree_api * tree;
    switch(tree_id) {
        case 1: {tree = (tree_api *) new btree::btree; break; }
        case 2: tree = (tree_api *) new btree_unsort::btree; break;
        case 3: tree = (tree_api *) new slotonly::wbtree; break;
        default: printf("Invalid tree type\n"); exit(-1);
    }
    std::vector<_key_t> keys(test_scale);
    std::default_random_engine e1(get_seed());
    std::uniform_int_distribution<_key_t> dist(0, 100000);
    for(int i = 0; i < test_scale; i++) {
        //keys[i] = dist(e1);
        keys[i] = i;
    }
    std::shuffle(keys.begin(), keys.end(), e1);
    
    cout << "put workload" << endl;
    put_throughput(tree, keys);

    cout << "get workload" << endl;
    get_throughput(tree, keys);

    std::shuffle(keys.begin(), keys.end(), e1);
    
    del_throughput(tree, keys);
    cout << "finish the test" << endl;

    delete tree;
    return 0;
}