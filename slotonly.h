/*  slotonly.h - btree of unordered tree node, but add a indirect array
    Copyright(c) 2020 Luo Yongping. All rights reserved.
*/

#include <stdio.h>
#include <cmath>
#include <iostream>
#include <vector>
#include <mutex>

#include "base.h"

namespace slotonly {
    using std::cout;
    using std::endl;

    const int CARDINALITY = 14; // No bigger than 15 (due to that the \
                    permutation is 64-bit long while each slot id is 4-bit long)
    const int UNDERFLOW_CARD = ceil((float)CARDINALITY / 2);
    
    
    /* Functions to access and modify the permutation array, that is slot array */
    static inline int8_t PERMUT_COUNT(const uint64_t p) { // cout the slot #
        return p & 0x0f;
    }

    static inline int8_t PERMUT_READ(const uint64_t p, int8_t idx) { // find the slot id of the idx-th key
        return (p & ((uint64_t)0xf << ((15 - idx) * 4))) >> ((15 - idx) * 4);
    }

    static inline int8_t PERMUT_ALLOC(const uint64_t p) { // allocate a free slot id 
        int8_t occupy[CARDINALITY] = {0};
        for(int i = 0; i < PERMUT_COUNT(p); i++) {
            occupy[PERMUT_READ(p, i)] = 1; 
        }
        for(int i = 0; i < CARDINALITY; i++) { // find a empty slot
            if(occupy[i] == 0) return i;
        }
        return 0;
    }

    static inline void PERMUT_ADD(uint64_t &p, int idx, int slot) { //add a new slot id at position idx
        int8_t num = PERMUT_COUNT(p);
        uint64_t tmp = (uint64_t)0xffffffffffffffff >> (idx * 4);
        uint64_t add_value = (uint64_t)slot << ((15 - idx) * 4);
        p = (p & ~tmp) + add_value + ((p & tmp) >> 4) + (num + 1);
    }

    static inline void PERMUT_DEL(uint64_t &p, int idx) { // delete a slot id at position idx
        int8_t num = PERMUT_COUNT(p);
        uint64_t tmp = (uint64_t) 0xffffffffffffffff >> (idx * 4 + 4);
        uint64_t del_mask = ~((uint64_t)0xf << ((15 - idx) * 4));
        
        p = (p & del_mask) - (p & tmp) + ((p & tmp - num) << 4) + (num - 1);
    }

    static inline void PERMUT_DELRIGHT(uint64_t &p, int idx) {
        uint64_t tmp = (uint64_t)0xffffffffffffffff >> (idx * 4);
        p = (p & ~tmp) + idx;
    }

    static inline void PERMUT_DELLEFT(uint64_t &p, int idx) {
        int8_t num = PERMUT_COUNT(p);
        uint64_t tmp = (uint64_t)0xffffffffffffffff >> (idx * 4 + 4);
        p = (((p - num) & tmp) << (idx * 4 + 4)) + (num - (idx + 1));
    }

    /* utility data type*/
    struct Record {
        _key_t key;
        char * val;
    };

    struct res_t { // a result type use to pass info when split and search
        bool flag; 
        Record rec;
        int8_t idx;
        res_t(bool f, Record e, int8_t i = -1) : flag(f), rec(e), idx(i) {}
    };

    class Node {
    private:
        uint64_t permutation; // 8 bytes
        char * leftmost_ptr; // 8 bytes
        char * sibling_ptr; // 8 bytes
        uint64_t *unused;    // 8 bytes
        Record recs[CARDINALITY];

        void insert_key(_key_t key, char * right) {
            int8_t num = PERMUT_COUNT(permutation);

            int8_t idx = 0, slot;
            do {
                slot = PERMUT_READ(permutation, idx); //find the first key in the node that geq key
            } while (key > recs[slot].key && ++idx < num);

            // alloc a slot in the node
            slot = PERMUT_ALLOC(permutation);
            recs[slot] = {key, right};
            // update the permutation array
            PERMUT_ADD(permutation, idx, slot);
        }

        void remove_key(int8_t idx) {
            PERMUT_DEL(permutation, idx);
        }

        _key_t borrow(Node * sib, _key_t uplevel_splitkey, bool borrow_from_right) {
            int8_t extra = leftmost_ptr != NULL ? 1 : 0;
            int8_t borrow_num = sib->card() - (sib->card() + card() + extra) / 2;
            _key_t new_splitkey;
            
            if(borrow_from_right) {
                if(leftmost_ptr == NULL) { // borrow from right leaf siblings
                    for(int i = 0; i < borrow_num; i++) {
                        int8_t slot = PERMUT_READ(sib->permutation, i);
                        insert_key(sib->recs[slot].key, sib->recs[slot].val);
                    }
                    new_splitkey = sib->get_key(borrow_num);
                } else {
                    insert_key(uplevel_splitkey, sib->leftmost_ptr);
                    for(int i = 0; i < borrow_num - 1; i++) {
                        int8_t slot = PERMUT_READ(sib->permutation, i);
                        insert_key(sib->recs[slot].key, sib->recs[slot].val);
                    }
                    new_splitkey = sib->get_key(borrow_num - 1);
                    sib->leftmost_ptr = sib->get_value(borrow_num - 1); // update leftmost_ptr of right node
                }  
                // update the right siblings
                PERMUT_DELLEFT(sib->permutation, borrow_num - 1);
                return new_splitkey;
            } else {
                int8_t borrow_start_idx = sib->card() - borrow_num;
                if(leftmost_ptr == NULL) { // merge with right leaf siblings
                    for(int i = sib->card() - 1; i >= borrow_start_idx; i--) {
                        int8_t slot = PERMUT_READ(sib->permutation, i);
                        insert_key(sib->recs[slot].key, sib->recs[slot].val);
                    }
                    new_splitkey = sib->get_key(borrow_start_idx);
                } else {
                    insert_key(uplevel_splitkey, leftmost_ptr);
                    for(int i = sib->card() - 1; i >= borrow_start_idx + 1; i--) {
                        int8_t slot = PERMUT_READ(sib->permutation, i);
                        insert_key(sib->recs[slot].key, sib->recs[slot].val);
                    }
                    new_splitkey = sib->get_key(borrow_start_idx);
                    leftmost_ptr = sib->get_value(borrow_start_idx);
                }
                // update the left siblings
                PERMUT_DELRIGHT(sib->permutation, borrow_start_idx);
                return new_splitkey;
            }
        }

        void merge(Node * sib, _key_t uplevel_splitkey, bool merge_with_right) {
        // the merge key is key from parent node
            if(merge_with_right) { // all the record in right siblings insert to current node
                if(leftmost_ptr != NULL) {
                    insert_key(uplevel_splitkey, sib->leftmost_ptr);
                }
                
                for(int i = 0; i < PERMUT_COUNT(sib->permutation); i++) {
                    int8_t slot = PERMUT_READ(sib->permutation, i);
                    insert_key(sib->recs[slot].key, sib->recs[slot].val);
                }
                sibling_ptr = sib->sibling_ptr;
            
            } else { // all the record in current node insert to left sibling  
                if(leftmost_ptr != NULL) {
                    sib->insert_key(uplevel_splitkey, leftmost_ptr);
                }
                
                for(int i = 0; i < PERMUT_COUNT(permutation); i++) {
                    int8_t slot = PERMUT_READ(permutation, i);
                    sib->insert_key(recs[slot].key, recs[slot].val); // insert record to sib
                }
                sib->sibling_ptr = sibling_ptr;
            }
        }

        void get_siblings(int8_t idx, Node * &left, Node * &right) const{
            if(idx == -1) {
                left = (Node *)NULL;
            } else if(idx == 0) {
                left = (Node *) leftmost_ptr;
            } else {
                left = (Node *) recs[PERMUT_READ(permutation, idx - 1)].val;
            }

            right = idx + 1 < PERMUT_COUNT(permutation) ? (Node *) recs[PERMUT_READ(permutation, idx + 1)].val : NULL;
        }

        inline _key_t get_key(int8_t idx) const{
            int8_t slot = PERMUT_READ(permutation, idx);
            return recs[slot].key;
        }

        inline char * get_value(int8_t idx) const {
            int8_t slot = PERMUT_READ(permutation, idx);
            return recs[slot].val;
        }

        inline void update_key(int8_t idx, _key_t key) {
            int8_t slot = PERMUT_READ(permutation, idx);
            recs[slot].key = key;
        }

        inline void update_value(int8_t idx, _value_t value) {
            int8_t slot = PERMUT_READ(permutation, idx);
            recs[slot].val = (char *)value;
        }

        inline bool underflow() const{
            return PERMUT_COUNT(permutation) < CARDINALITY / 2;
        }

        inline int8_t card() const{
            return PERMUT_COUNT(permutation);
        }

    public:
        friend class wbtree;
        
        Node() :permutation(0), leftmost_ptr(NULL), sibling_ptr(NULL) {}

        void * operator new(size_t size) {
            #ifdef _WIN32
                void *ret = _aligned_malloc(size, 64); 
            #else
                void * ret;
                if(posix_memalign(&ret,64,size) != 0)
                    exit(-1);
            #endif
            return ret;
        }
        
        void operator delete(void * ptr) {
            if(ptr != NULL) {
                Node * this_node = (Node *) ptr;
                if(this_node->leftmost_ptr != NULL) {
                    delete (Node *)this_node->leftmost_ptr;
                    for(int i = 0; i < PERMUT_COUNT(this_node->permutation); i++) {
                        int8_t slot = PERMUT_READ(this_node->permutation, i++);
                        delete (Node *)this_node->recs[slot].val;
                    }
                } 
                #ifdef _WIN32
                    _aligned_free(ptr);
                #else
                    free(ptr);
                #endif
            }
        }

        res_t store(_key_t key, char * right) {
        // if split, return with a true flag and return the split key along with the address of the new node
            int num_entries = PERMUT_COUNT(permutation);

            if(num_entries < CARDINALITY) {
                insert_key(key, right);

                return res_t(false, {0, NULL});
            } else { // split the node here 
                Node * new_node = new Node();
                int8_t right_num = std::ceil((float)num_entries / 2);
                int8_t m = num_entries - right_num;

                int8_t slot = PERMUT_READ(permutation, m);
                if(key >= recs[slot].key) { // make the split more even 
                    slot = PERMUT_READ(permutation, ++m);
                    right_num -= 1;
                }
                _value_t split_key = recs[slot].key;// record the splitkey
                
                //copy records to the new node
                if(leftmost_ptr != NULL) {
                    new_node->leftmost_ptr = recs[slot].val;
                    m += 1;
                }
                int8_t new_slot = 0;
                do { 
                    slot = PERMUT_READ(permutation, m++); //find (m+1)-th record in this node
                    new_node->recs[new_slot] = recs[slot];
                    PERMUT_ADD(new_node->permutation, new_slot, new_slot);
                    new_slot += 1;
                } while (m < num_entries);

                if(leftmost_ptr == NULL) { //leaf node, update the sibling node
                    new_node->sibling_ptr = sibling_ptr;
                }
                
                sibling_ptr = (char *)new_node;
                //update the permutation of the original node
                PERMUT_DELRIGHT(permutation, num_entries - right_num);    

                // insert the key-value after the splitting
                if(key < split_key) {
                    insert_key(key, right);
                } else {
                    new_node->insert_key(key, right);
                }

                return res_t(true, {split_key, (char *)new_node});
            }
        }

        bool remove(_key_t key) {
            int num = PERMUT_COUNT(permutation);

            int8_t idx = 0, slot;
            do {
                slot = PERMUT_READ(permutation, idx); //find the first key in the node that geq key
            } while (key > recs[slot].key && ++idx < num);

            if(recs[slot].key == key) {
                remove_key(idx);
            } else {
                printf("Key:%ld Not Found\n", key);
            }

            if(num > ceil((float)CARDINALITY / 2)) {
                return false;
            } else { // need to merge with siblings
                return true;
            }
        }

        res_t linear_search(_key_t key) const {
        // if found, return with a true flag, or with a false flag
            int8_t num = PERMUT_COUNT(permutation);

            if(leftmost_ptr == NULL) { // leaf node
                int8_t idx = 0, slot;
                do {
                    slot = PERMUT_READ(permutation, idx); //find the first key in the node that geq key
                } while (key > recs[slot].key && ++idx < num);

                Record rec = recs[slot];
                
                // find a record's key in the node equals key or till the last record
                if(recs[slot].key == key) 
                    return res_t(true, recs[slot], idx);
                else 
                    return res_t(false, {0, NULL}, idx);
            } else { // inner node
                if(num == 0 || key < recs[PERMUT_READ(permutation, 0)].key) {
                    return res_t(true, {key, leftmost_ptr}, -1);
                }
                int8_t idx = 0, slot;
                if(num == 1) {
                    slot = PERMUT_READ(permutation, 0);
                } else { // fix a bug here, in the following way, we can find the right slot
                         // when num equls 1
                    do { 
                        slot = PERMUT_READ(permutation, idx + 1);
                    } while (key >= recs[slot].key && ++idx < num - 1); // find the first record whose key gt key, the record before that is the target
                    
                    slot = PERMUT_READ(permutation, idx);
                }
                
                return res_t(true, recs[slot], idx);
            }
        }

        void print(const int8_t tree_depth, int8_t cur_depth, bool recursively) const {
            std::string prefix = "";
            for(int i = 0; i < cur_depth; i++) {
                prefix += "  ";
            } 
            cout << prefix <<"Node(" << tree_depth - cur_depth << ") at " << (uint64_t)this<< " Left Ptr:"<< (uint64_t)leftmost_ptr 
                << " Sibling Ptr:"<< (uint64_t)sibling_ptr;
            printf(" Permutation: 0x%08lx%08lx ", permutation / ((uint64_t)1 << 32), permutation % ((uint64_t)1 << 32));
            for(int i = 0; i < PERMUT_COUNT(permutation); i++) {
                int8_t slot = PERMUT_READ(permutation, i);
                cout << "(" << recs[slot].key << "," << uint64_t(recs[slot].val) << ") ";
            }
            cout << endl;
            
            if(leftmost_ptr != NULL && recursively) {
                ((Node *)leftmost_ptr)->print(tree_depth, cur_depth + 1, true);
                for(int i = 0; i < PERMUT_COUNT(permutation); i++) {
                    int8_t slot = PERMUT_READ(permutation, i);
                    ((Node *)recs[slot].val)->print(tree_depth, cur_depth + 1, true);
                }
            }
        }

        void clear() {
        // clear all the record and free the node
            permutation = 0;
            leftmost_ptr = NULL;
            sibling_ptr = NULL;

            delete this;
        }
    };

    class wbtree : tree_api {
    private:
        int8_t tree_height;
        Node * root;

        res_t insert_recursive(Node * n, _key_t k, _value_t v) {
            if(n->leftmost_ptr == NULL) {
                return n->store(k, (char *)v);
            } else {
                res_t find_res = n->linear_search(k); // find the child node

                res_t insert_res = insert_recursive((Node *)find_res.rec.val, k, v);

                if(insert_res.flag == true) { // splitting cascades to Node n
                    return n->store(insert_res.rec.key, insert_res.rec.val);
                } else {
                    return res_t(false, {0, NULL});
                }
            }
        }

        bool remove_recursive(Node * n, _key_t k) {
            if(n->leftmost_ptr == NULL) { //leaf node
                return n->remove(k);
            } else { //inner node
                res_t find_res = n->linear_search(k);
                Node * child = (Node *)find_res.rec.val;

                bool isUnderflow = remove_recursive(child, k);
                if(isUnderflow == true) { // the child node has splitted
                    Node * leftchild, *rightchild;
                    n->get_siblings(find_res.idx, leftchild, rightchild);

                    if(leftchild != NULL && leftchild->card() > UNDERFLOW_CARD) {
                        _key_t cur_key = n->get_key(find_res.idx);
                        _key_t new_key = child->borrow(leftchild, cur_key, false);
                        
                        n->update_key(find_res.idx, new_key);

                        return false;
                    } else if(rightchild != NULL && rightchild->card() > UNDERFLOW_CARD) {
                        _key_t right_key = n->get_key(find_res.idx + 1);
                        _key_t new_key = child->borrow(rightchild, right_key, true);
                        
                        n->update_key(find_res.idx + 1, new_key);

                        return false;
                    } else if (leftchild != NULL){
                        _key_t cur_key = n->get_key(find_res.idx);
                        child->merge(leftchild, cur_key, false);
                        child->clear();

                        n->remove_key(find_res.idx);
                        return n->underflow();
                    } else { // if child has no left sibling, it must have a right sibling
                        _key_t right_key = n->get_key(find_res.idx + 1);
                        child->merge(rightchild, right_key, true);
                        rightchild->clear();;

                        n->remove_key(find_res.idx + 1);

                        return n->underflow();
                    }
                } 
                return false;
            }
        }

    public:
        wbtree() {
            root = new Node();
            tree_height = 1;
        }

        ~wbtree() {
            delete root; //Node deconstrution will automatically free the child node
        }
    
        bool find(_key_t k, _value_t &v) {
            Node * cur = root;
            
            while(cur->leftmost_ptr != NULL) {
                res_t find_res = cur->linear_search(k);
                cur = (Node *)find_res.rec.val;
            }

            res_t find_res = cur->linear_search(k);
            if(find_res.flag == true) {
                v = (_value_t)find_res.rec.val;
                return true;
            } else {
                return false;
            }
        }
        

        void insert(_key_t k, _value_t v) {
        // if tree level in the threshold, return false, else return the splited new root
            res_t insert_res = insert_recursive(root, k, v);

            if(insert_res.flag == true) { // splitting cascades to the root node
                Node * new_root = new Node();

                new_root->leftmost_ptr = (char *) root;
                
                new_root->store(insert_res.rec.key, insert_res.rec.val);

                root = new_root;

                tree_height += 1;
            }

            return ;
        }

        bool update(_key_t k, _value_t v) {
            Node * cur = root;
                
            while(cur->leftmost_ptr != NULL) {
                res_t find_res = cur->linear_search(k);
                cur = (Node *)find_res.rec.val;
            }

            res_t find_res = cur->linear_search(k);
            if(find_res.flag == true) { // we find the value in the tree
                cur->update_value(find_res.idx, v);
                return true;
            }
            return false;
        }

        bool remove(_key_t k) {
        // if no more record, return false
            if(root->leftmost_ptr == NULL) { // root node is a leaf node
                root->remove(k);
                
                return root->card() > 0;
            } else {
                res_t find_res = root->linear_search(k);
                
                Node * child = (Node *)find_res.rec.val;
                Node * leftchild, *rightchild;

                bool isUnderflow = remove_recursive(child, k);
                if(isUnderflow == true) {
                    Node * leftchild, *rightchild;
                    root->get_siblings(find_res.idx, leftchild, rightchild);

                    if(leftchild != NULL && leftchild->card() > UNDERFLOW_CARD) {
                        _key_t cur_key = root->get_key(find_res.idx);
                        _key_t new_key = child->borrow(leftchild, cur_key, false);

                        root->update_key(find_res.idx, new_key);

                    } else if(rightchild != NULL && rightchild->card() > UNDERFLOW_CARD) {
                        _key_t right_key = root->get_key(find_res.idx + 1);
                        _key_t new_key = child->borrow(rightchild, right_key, true);

                        root->update_key(find_res.idx + 1, new_key);
                    } else if (leftchild != NULL){
                        _key_t cur_key = root->get_key(find_res.idx);
                        child->merge(leftchild, cur_key, false);
                        child->clear();

                        root->remove_key(find_res.idx);
                    } else if(rightchild != NULL){
                        _key_t right_key = root->get_key(find_res.idx + 1);
                        child->merge(rightchild, right_key, true);
                        rightchild->clear();
                        root->remove_key(find_res.idx + 1);
                    } 
                    
                    //if root has no key, make the only child be the root
                    if(root->card() == 0) { // that is able to recover
                        root = (Node *) root->leftmost_ptr;
                    }
                }
                return true;
            }
        }

        void printAll() {
            root->print(tree_height, 0, true);
        }
    };
};