/*  btree.h - a in-memory btree implementation
    Copyright(c) 2020 Luo Yongping. All rights reserved.
*/
#ifndef __BTREE__
#define __BTREE__

#include <cstdint>
#include <cstring>
#include <string>
#include <cstdio>
#include <random>

#include "base.h"

namespace btree {
using std::string;

const int PAGESIZE = 512;
const int NODE_SIZE = (PAGESIZE - 32) / 16;

struct Record {
    _key_t key;
    char * val;
};

class Node {
    public:
        char * leftmost_ptr;
        uint64_t count;
        char hdr[16]; // the total meta data is 32 bytes
        Record recs[NODE_SIZE];
    
    public:
        Node (): leftmost_ptr(NULL), count(0){}
        
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
                    for(int i = 0; i < this_node->count; i++) {
                        delete (Node *)this_node->recs[i].val;
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
                uint64_t i;
                for(i = 0; i < count; i++) {
                    if(recs[i].key >= key) {
                        break;
                    }
                }

                if (recs[i].key == key)
                    return recs[i].val;
                else // recs[i].key > key, not found
                    return NULL;
            } else {
                uint64_t i;
                for(i = 0; i < count; i++) {
                    if(recs[i].key > key) {
                        break;
                    }
                }

                if(i == 0)
                    return leftmost_ptr;
                else // recs[i - 1].key <= key
                    return recs[i - 1].val;
            }
        }

        void insert(_key_t k, _value_t v) {
            uint64_t i;
            for(i = 0; i < count; i++) {
                if(recs[i].key > k) {
                    break;
                }
            }

            // recs[i].key <= key
            memmove(&recs[i + 1], &recs[i], sizeof(Record) * (count - i));

            recs[i] = {k, (char *) v};

            count += 1;
        }

        bool store(_key_t k, _value_t v, _key_t & split_k, Node * & split_node) {
            if(count == NODE_SIZE) {
                split_node = new Node;

                uint64_t m = count / 2;
                split_k = recs[m].key;

                if(leftmost_ptr == NULL) {
                    split_node->count = count - m;
                    memcpy(&(split_node->recs[0]), &(recs[m]), sizeof(Record) * (split_node->count));
                } else {
                    split_node->leftmost_ptr = recs[m].val;

                    split_node->count = count - m - 1;
                    memcpy(&(split_node->recs[0]), &(recs[m + 1]), sizeof(Record) * (split_node->count));
                }
                count = m;

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
            printf("%s[ ", prefix.c_str());
            for(int i = 0; i < count; i++) {
                printf("(%ld, %ld) ", recs[i].key, (int64_t)recs[i].val);
            }
            printf("]\n");

            if(leftmost_ptr != NULL) {
                Node * child = (Node *)leftmost_ptr;
                child->print(prefix + "    ");

                for(int i = 0; i < count; i++) {
                    Node * child = (Node *)recs[i].val;
                    child->print(prefix + "    ");
                }
            }
        }
};

class btree : tree_api{
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
            while(cur->leftmost_ptr != NULL) { // no prefetch here
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
                Node * child = (Node *)n->get_child(k);
                
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
