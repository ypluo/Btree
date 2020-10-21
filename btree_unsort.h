/*  unsort_btree.h - a in-memory btree (unsort node) implementation
    Copyright(c) 2020 Luo Yongping. All rights reserved.
*/

#ifndef __UNSORT_BTREE__
#define __UNSORT_BTREE__

#include <cstdint>
#include <cstring>
#include <string>
#include <cstdio>
#include <random>
#include <queue>
#include <functional>

#include "base.h"

namespace btree_unsort {

using std::string;

const int PAGESIZE = 512;
const int NODE_SIZE = ((PAGESIZE - 32) / 16);

struct Record {
    _key_t key;
    char * val;
};

class Node {
    public:
        char * leftmost_ptr;
        uint64_t count;
        uint64_t bitmap;
        char hdr[8]; // the total meta data is 32 bytes
        Record recs[NODE_SIZE];
    
    public:
        Node (): leftmost_ptr(NULL), count(0), bitmap(0) {}
        
        void * operator new (size_t size) {
            #ifdef _WIN32
                void *ret = _aligned_malloc(size, 64); 
            #else
                void * ret;
                if(posix_memalign(&ret,64,size) != 0)
                    exit(-1);
            #endif
        }

        void operator delete(void * ptr) {
            if(ptr != NULL) {
                Node * this_node = (Node *) ptr;
                if(this_node->leftmost_ptr != NULL) {
                    delete (Node *)this_node->leftmost_ptr;

                    uint64_t mask = 0x8000000000000000;
                    for(int i = 0; i < NODE_SIZE; i++) {
                        if((this_node->bitmap & mask) > 0) {
                            delete (Node *)this_node->recs[i].val;
                        }
                        mask >>= 1;
                    }
                } 

                #ifdef _WIN32
                    _aligned_free(ptr);
                #else
                    free(ptr);
                #endif
            }
        }

        char * get_child(_key_t key) {
            if(leftmost_ptr == NULL) {
                uint64_t mask = 0x8000000000000000;
                for(int i = 0; i < NODE_SIZE; i++) {
                    if((bitmap & mask) > 0 && recs[i].key == key) {
                        return recs[i].val;
                    }
                    mask >>= 1;
                }
                return NULL;
            } else {
                _key_t max_leqkey = -1;
                int8_t max_leqi = -1;
                
                uint64_t mask = 0x8000000000000000;
                for(int i = 0; i < NODE_SIZE; i++) {
                    if((bitmap & mask) > 0) {
                        if(recs[i].key <= key && max_leqkey < recs[i].key) {
                            max_leqkey = recs[i].key;
                            max_leqi = i;
                        }
                    }
                    mask >>= 1;
                }

                if(max_leqi == -1) {
                    return leftmost_ptr;
                } else {
                    return recs[max_leqi].val;
                }

                return NULL;
            }
        }

        void insert(_key_t k, _value_t v) {
            uint64_t mask = 0x8000000000000000;
            int8_t slot;
            for(int i = 0; i < NODE_SIZE; i++) {
                if((bitmap & mask) == 0) {
                    slot = i;
                    break;
                }
                mask >>= 1;
            }
    
            recs[slot] = {k, (char *)v};

            count += 1;
            bitmap |= mask;
        }

        bool store(_key_t k, _value_t v, _key_t & split_k, Node * & split_node) {
            if(count == NODE_SIZE) {
                split_node = new Node;

                split_k = get_median();
                int8_t j = 0;
                
                uint64_t mask = 0x8000000000000000;
                if(leftmost_ptr == NULL) {
                    for(int i = 0; i < NODE_SIZE; i++) {
                        if(recs[i].key >= split_k) {
                            bitmap &= (~mask);
                            split_node->recs[j++] = recs[i];
                        }
                        mask >>= 1;
                    }
                } else {
                    for(int i = 0; i < NODE_SIZE; i++) {
                        if(recs[i].key > split_k) {
                            bitmap &= (~mask);
                            split_node->recs[j++] = recs[i];
                        } else if(recs[i].key == split_k) {
                            bitmap &= (~mask);
                            split_node->leftmost_ptr = recs[i].val;
                        }
                        mask >>= 1;
                    }
                }

                split_node->count = j;
                split_node->bitmap = UINT64_MAX << (64 - j);
                
                count -= j + (leftmost_ptr == NULL ? 0 : 1);                

                if(split_k > k) {
                    insert(k, v);
                } else {
                    split_node->insert(k, v);
                }
                return true;
            } else {
                insert(k, v);

                return false;
            }
        }

        void print(string prefix) {
            printf("%s(%ld, %lx)[ ", prefix.c_str(), count, bitmap);
            uint64_t mask = 0x8000000000000000;
            for(int i = 0; i < NODE_SIZE; i++) {
                if((bitmap & mask) > 0) {
                    printf("(%ld, %ld) ", recs[i].key, (int64_t)recs[i].val);
                }
                mask >>= 1;
            }
            printf("]\n");

            if(leftmost_ptr != NULL) {
                Node * child = (Node *)leftmost_ptr;
                child->print(prefix + "    ");

                uint64_t mask = 0x8000000000000000;
                for(int i = 0; i < NODE_SIZE; i++) {
                    if((bitmap & mask) > 0) {
                        Node * child = (Node *)recs[i].val;
                        child->print(prefix + "    ");
                    }
                    mask >>= 1;
                }
            }
        }
    
        _key_t get_median() {
            std::priority_queue<_key_t, std::vector<_key_t>> q; // max heap

            for(int i = 0; i <= NODE_SIZE / 2; i++) {
                q.push(recs[i].key);
            }

            for(int i = NODE_SIZE / 2 + 1; i < NODE_SIZE; i++) {
                if(q.top() > recs[i].key) {
                    q.pop();
                    q.push(recs[i].key);
                }
            }

            // now in the priority queue, store the first half of the keys
            return q.top();
        }
};

class btree : tree_api {
    private:
        Node * root;

    public:
        btree() {
            root = new Node;
        }

        ~btree() {
            delete root;
        }

        bool find(_key_t key, _value_t &val) {
            Node * cur = root;
            while(cur->leftmost_ptr != NULL) {
                char * child_ptr = cur->get_child(key);
                cur = (Node *)child_ptr;
            }

            val = (_value_t) cur->get_child(key);

            return true;
        }

        void insert(_key_t key, _value_t val) {
            _key_t split_k;
            Node * split_node;
            bool splitIf = insert_recursive(root, key, val, split_k, split_node);

            if(splitIf) {
                Node *new_root = new Node;
                new_root->leftmost_ptr = (char *)root;
                new_root->recs[0].val = (char *)split_node;
                new_root->recs[0].key = split_k;
                new_root->count = 1;
                new_root->bitmap = (0x8000000000000000);

                root = new_root;
            }
        }

        bool update(_key_t key, _value_t value) { // TODO: not implemented
            return false;
        }

        bool remove(_key_t key) { // TODO: not implemented
            return false;
        }

        void printAll() {
            root->print(string(""));
        }

    private:
        bool insert_recursive(Node * n, _key_t k, _value_t v, _key_t &split_k, Node * &split_node) {
            if(n->leftmost_ptr == NULL) {
                return n->store(k, v, split_k, split_node);
            } else {
                Node * child = (Node *) n->get_child(k);
                
                _key_t split_k_child;
                Node * split_node_child;
                bool splitIf = insert_recursive(child, k, v, split_k_child, split_node_child);

                if(splitIf) { 
                    return n->store(split_k_child, (_value_t)split_node_child, split_k, split_node);
                } 
                return false;
            }
        }
}; // class btree

}; // namespace btree

#endif
