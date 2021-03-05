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

const int PAGESIZE = 256;
const int NODE_SIZE = (PAGESIZE - 32) / 16;

struct Record {
    _key_t key;
    char * val;
};

class Node {
    public:
        char * leftmost_ptr; // NULL means the node is a leaf node; Non-null value represents the leftmost child of current node
        char * sibling_ptr;
        uint64_t count;      // total record number in current node
        char dummy[8];       // the total meta data is 32 bytes
        Record recs[NODE_SIZE];
    private:
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
    
    public:
        Node (): leftmost_ptr(NULL), sibling_ptr(NULL), count(0){}
        
        void * operator new (size_t size) { // make the allocation 64 B aligned
            #ifdef _WIN32
                void *ret = _aligned_malloc(size, 64); 
            #else
                void * ret;
                if(posix_memalign(&ret,64,size) != 0)
                    exit(-1);
                return ret;
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

        char * get_child(_key_t key) { // find the record whose key is the last one that is less equal to key
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

        bool store(_key_t k, _value_t v, _key_t & split_k, Node * & split_node) {
            if(count == NODE_SIZE) {
                split_node = new Node;

                uint64_t m = count / 2;
                split_k = recs[m].key;
                // move half records into the new node
                if(leftmost_ptr == NULL) {
                    split_node->count = count - m;
                    memcpy(&(split_node->recs[0]), &(recs[m]), sizeof(Record) * (split_node->count));
                } else {
                    split_node->leftmost_ptr = recs[m].val;

                    split_node->count = count - m - 1;
                    memcpy(&(split_node->recs[0]), &(recs[m + 1]), sizeof(Record) * (split_node->count));
                }
                count = m;

                // update sibling pointer
                split_node->sibling_ptr = sibling_ptr;
                sibling_ptr = (char *) split_node;

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

        bool remove(_key_t k) { // remove k from current node
            int pos = -1;
            for(int i = 0; i < count; i++) {
                if(recs[i].key == k){
                    pos = i;
                    break;
                }
            }
            if(pos >= 0) { // we found k in this node
                memmove(&recs[pos], &recs[pos + 1], sizeof(Record) * (count - pos - 1));
                count -= 1;
                return true;
            } 
            return false;
        }

        int get_lrchild(_key_t k, Node * & left, Node * & right) {
            int16_t i = 0;
            for( ; i < count; i++) {
                if(recs[i].key > k)
                    break;
            }

            if(i == 0) {
                left = NULL;
            } else if(i == 1) {
                left = (Node *)leftmost_ptr;
            } else {
                left = (Node *)recs[i - 2].val;
            }

            if(i == count) {
                right = NULL;
            } else {
                right = (Node *)recs[i].val;
            }
            return i;
        }
    
        static void merge(Node * left, Node * right, _key_t merge_key) {
            if(left->leftmost_ptr == NULL) {
                for(int i = 0; i < right->count; i++) {
                    left->recs[left->count++] = right->recs[i];
                }
            } else {
                left->recs[left->count++] = {merge_key, right->leftmost_ptr}; 
                for(int i = 0; i < right->count; i++) {
                    left->recs[left->count++] = right->recs[i];
                }
            }
            free((void *)right);
        }

        void print(string prefix) {
            printf("%s[(%lu) ", prefix.c_str(), count);
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

        bool remove(_key_t key) {   
            if(root->leftmost_ptr == NULL) {
                root->remove(key);

                return root->count == 0;
            }
            else {
                Node * child = (Node *) root->get_child(key);

                bool shouldMrg = remove_recursive(child, key);

                if(shouldMrg) {
                    Node *leftsib = NULL, *rightsib = NULL;
                    int pos = root->get_lrchild(key, leftsib, rightsib);

                    if(leftsib != NULL && (child->count + leftsib->count) < NODE_SIZE) {
                        // merge with left node
                        _key_t merge_key = root->recs[pos - 1].key;
                        root->remove(merge_key);
                        Node::merge(leftsib, child, merge_key);
                    } 
                    else if (rightsib != NULL && (child->count + rightsib->count) < NODE_SIZE) {
                        // merge with right node
                        _key_t merge_key = root->recs[pos].key;
                        root->remove(merge_key);
                        Node::merge(child, rightsib, merge_key);
                    }
                    
                    if(root->count == 0) { // the root is empty
                        Node * old_root = root;

                        root = (Node *)root->leftmost_ptr;
                        
                        old_root->leftmost_ptr = NULL;
                        delete old_root;
                    }
                }

                return false;
            } 
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

        bool remove_recursive(Node * n, _key_t k) {
            if(n->leftmost_ptr == NULL) {
                n->remove(k);
                return n->count <= NODE_SIZE / 3;
            }
            else {
                Node * child = (Node *) n->get_child(k);

                bool shouldMrg = remove_recursive(child, k);

                if(shouldMrg) {
                    Node *leftsib = NULL, *rightsib = NULL;
                    int pos = n->get_lrchild(k, leftsib, rightsib); // pos > 0 or left_sib == NULL

                    if(leftsib != NULL && (child->count + leftsib->count) < NODE_SIZE) {
                        // merge with left node
                        _key_t merge_key = n->recs[pos - 1].key;
                        n->remove(merge_key);
                        Node::merge(leftsib, child, merge_key);
                        
                        return n->count <= NODE_SIZE / 3;
                    } else if (rightsib != NULL && (child->count + rightsib->count) < NODE_SIZE) {
                        // merge with right node
                        _key_t merge_key = n->recs[pos].key;
                        n->remove(merge_key);
                        Node::merge(child, rightsib, merge_key);
                        
                        return n->count <= NODE_SIZE / 3;
                    }
                }
                return false;
            }
        }

}; // class btree

}; // namespace btree

#endif
