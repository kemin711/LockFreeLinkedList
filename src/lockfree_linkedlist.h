#ifndef LOCKFREE_LINKEDLIST_H
#define LOCKFREE_LINKEDLIST_H

#include <atomic>
#include <cstdio>
#include <iostream>

//#include "HazardPointer/reclaimer.h"
#include "reclaimer.h"

template <typename T, typename cmp = std::less<T> >
class ListReclaimer;

/**
 * Lockfree single linked sorted list
 * use T.getid() method to compare T
 */
template <typename T, typename cmp = std::less<T> > 
class LockFreeLinkedList {
   static_assert(std::is_copy_constructible_v<T>, "T requires copy constructor");
   friend ListReclaimer<T,cmp>;

   /**
    * Inner class
    */
   struct Node {
      /**
       * Default constructor
       */
      Node() : data(nullptr), next(nullptr) {};

      template <typename... Args>
      Node(Args&&... args)
         : data(new T(std::forward<Args>(args)...)), next(nullptr) {}
      bool empty() const {
         return data == nullptr;
      }

      ~Node() {
         if (data != nullptr) delete data;
      };

      /**
       * The actualy data of the node is a pointer to T
       * Maybe better to use shared_ptr that can improve the
       * safety of operation and more atomic.
       */
      T* data;
      /**
       * Next pointer
       */
      std::atomic<Node*> next;
   };


 public:
   /**
    * Default constructor
    */
   LockFreeLinkedList() : head_(new Node()), size_(0) {}
   /**
    * Disabled copy constructor
    */
   LockFreeLinkedList(const LockFreeLinkedList& other) = delete;
   LockFreeLinkedList(LockFreeLinkedList&& other) = delete;
   LockFreeLinkedList& operator=(const LockFreeLinkedList& other) = delete;
   LockFreeLinkedList& operator=(LockFreeLinkedList&& other) = delete;

   ~LockFreeLinkedList() {
      Node* p = head_;
      while (p != nullptr) {
         Node* tmp = p;
         p = p->next.load(std::memory_order_acquire);
         // We can safely delete node, because each thread exits before list
         // destruct, while thead exiting, it wait all hazard pointers hand over.
         delete tmp;
      }
   }

   /**
    * Find the first node which data is greater than the given data,
    * then insert the new node before it then return true, else if
    * data is already exist in list then return false.
    * @param args is variable number of inputs for constructor of T.
    * @return true if inserted new nodes, otherwise node
    *    exists and return false.
    */
   template <typename... Args> bool Emplace(Args&&... args);
   /**
    * Insert a copy of data
    */
   bool Insert(const T& data) {
      static_assert(std::is_copy_constructible<T>::value,
                     "T must be copy constructible");
      return Emplace(data);
   }
   /**
    * Insert data by moving it
    */
   bool Insert(T&& data) {
      static_assert(std::is_constructible_v<T, T&&>,
                     "T must be constructible with T&&");
      return Emplace(std::forward<T>(data));
   }
   /** 
    * Find the first node whose data is equals to the given data,
    * then delete it and return true, if not found the given data then
    * return false.
    */
   bool Delete(const T& data);
   /**
    * Find the first node which data is equals to the given data, if not found
    * the given data then return false.
    */
   bool Find(const T& data) {
      Node* prev;
      Node* cur;
      HazardPointer prev_hp, cur_hp;
      bool found = Search(data, &prev, &cur, prev_hp, cur_hp);
      return found;
   }
   /** 
    * Get size of the list.
    */
   size_t size() const { return size_.load(std::memory_order_relaxed); }
   /**
    * Only a single thread can call this function.
    * @return a pointer to the data field of the head node
    */
   T* getHeadData() const {
      if (head_ == nullptr) {
         //std::cerr << "head_ is null!\n";
         return nullptr;
      }
      else {
         //std::cerr << "head_=" << head_ << " head_->data=" << head_->data 
         //   << " head_->next=" << head_->next << std::endl;
         if (head_->next != nullptr) {
            Node* x=head_->next.load();
            //std::cerr << " next=" << x << " x->data=" << x->data << std::endl;
            return x->data;
         }
         return nullptr;
      }
   }
   bool deleteHead() {
      const T& href(*getHeadData());
      return Delete(href);
   }
   /**
    * Debug function to show the content.
    * Only one thread should call this one without 
    * other threads updating this structure.
    */
   void show(std::ostream& ous) const;

 private:
   bool InsertNode(Node* new_node);

   /**
    * Find the first node which data is equals to the given data, if not found
    * the given data then return false. *cur_ptr point to that node, *prev_ptr is
    * the predecessor of that node.
    * @param data the data value reference
    * @param prev_ptr pointer to pointer to previous node will be updated.
    * @param cur_ptr pointer to pointer to current node will be update
    *    to the node found
    * @return true if found, otherwise false
    */
   bool Search(const T& data, Node** prev_ptr, Node** cur_ptr,
               HazardPointer& prev_hp, HazardPointer& cur_hp);

   //bool Less(const T& data1, const T& data2) const { return data1 < data2; }

   bool GreaterOrEquals(const T& data1, const T& data2) const {
      //return !(Less(data1, data2));
      return !(cmp()(data1, data2));
   }

   bool Equals(const T& data1, const T& data2) const {
      //return !Less(data1, data2) && !Less(data2, data1);
      return !cmp()(data1, data2) && !cmp()(data2, data1);
   }

   bool is_marked_reference(Node* next) const {
      return (reinterpret_cast<unsigned long>(next) & 0x1) == 0x1;
   }

   Node* get_marked_reference(Node* next) const {
      return reinterpret_cast<Node*>(reinterpret_cast<unsigned long>(next) | 0x1);
   }

   Node* get_unmarked_reference(Node* next) const {
      return reinterpret_cast<Node*>(reinterpret_cast<unsigned long>(next) & ~0x1);
   }

   static void OnDeleteNode(void* ptr) { delete static_cast<Node*>(ptr); }

   /// private members ///
   /**
    * Head of the node and the whole list
    */
   Node* head_;
   /**
    * The total number of nodes in the list
    */
   std::atomic<size_t> size_;
   /**
    * For the whole class
    */
   static Reclaimer::HazardPointerList global_hp_list_;
};

/// class implementation ////

template <typename T, typename cmp>
Reclaimer::HazardPointerList LockFreeLinkedList<T, cmp>::global_hp_list_;

// helper class
template <typename T, typename cmp>
class ListReclaimer : public Reclaimer {
   friend LockFreeLinkedList<T,cmp>;

   private:
      ListReclaimer(HazardPointerList& hp_list) : Reclaimer(hp_list) {}
      ~ListReclaimer() override = default;

      static ListReclaimer<T,cmp>& GetInstance() {
         thread_local static ListReclaimer reclaimer(
            LockFreeLinkedList<T,cmp>::global_hp_list_);
         return reclaimer;
      }
};

template <typename T, typename cmp>
template <typename... Args>
bool LockFreeLinkedList<T,cmp>::Emplace(Args&&... args) {
   Node* new_node = new Node(std::forward<Args>(args)...);
   Node* prev;
   Node* cur;
   HazardPointer prev_hp, cur_hp;
   do {
      if (Search(*new_node->data, &prev, &cur, prev_hp, cur_hp)) {
         // List already contains *new_node->data.
         delete new_node;
         return false;
      }
      new_node->next.store(cur, std::memory_order_release);
   } while (!prev->next.compare_exchange_weak(
         cur, new_node, std::memory_order_release, std::memory_order_relaxed));

   size_.fetch_add(1, std::memory_order_relaxed);
   return true;
}

template <typename T, typename cmp>
bool LockFreeLinkedList<T,cmp>::Delete(const T& data) {
   Node* prev;
   Node* cur;
   Node* next;
   HazardPointer prev_hp, cur_hp;
   do {
      do {
         if (!Search(data, &prev, &cur, prev_hp, cur_hp)) {
            return false;
         }
         next = cur->next.load(std::memory_order_acquire);
      } while (is_marked_reference(next));
      // Logically delete cur by marking cur->next.
   } while (!cur->next.compare_exchange_weak(next, get_marked_reference(next),
                                             std::memory_order_release,
                                             std::memory_order_relaxed));

   if (prev->next.compare_exchange_strong(cur, next, std::memory_order_release,
                                          std::memory_order_relaxed)) {
      size_.fetch_sub(1, std::memory_order_relaxed);
      auto& reclaimer = ListReclaimer<T,cmp>::GetInstance();
      reclaimer.ReclaimLater(cur, LockFreeLinkedList<T,cmp>::OnDeleteNode);
      reclaimer.ReclaimNoHazardPointer();
   } else {
      prev_hp.UnMark();
      cur_hp.UnMark();
      Search(data, &prev, &cur, prev_hp, cur_hp);
   }

   return true;
}

template <typename T, typename cmp>
bool LockFreeLinkedList<T,cmp>::Search(const T& data, Node** prev_ptr,
           Node** cur_ptr, HazardPointer& prev_hp, HazardPointer& cur_hp) 
{
   auto& reclaimer = ListReclaimer<T,cmp>::GetInstance();
try_again:
   Node* prev = head_;
   Node* cur = prev->next.load(std::memory_order_acquire);
   Node* next;
   while (true) {
      cur_hp.UnMark();
      cur_hp = HazardPointer(&reclaimer, cur);
      // Make sure prev is the predecessor of cur,
      // so that cur is properly marked as hazard.
      if (prev->next.load(std::memory_order_acquire) != cur) goto try_again;

      if (nullptr == cur) {
         *prev_ptr = prev;
         *cur_ptr = cur;
         return false;
      };

      next = cur->next.load(std::memory_order_acquire);
      if (is_marked_reference(next)) {
         if (!prev->next.compare_exchange_strong(cur, get_unmarked_reference(next)))
            goto try_again;
         reclaimer.ReclaimLater(cur, LockFreeLinkedList<T,cmp>::OnDeleteNode);
         reclaimer.ReclaimNoHazardPointer();
         size_.fetch_sub(1, std::memory_order_relaxed);
         cur = get_unmarked_reference(next);
      } 
      else {
         const T& cur_data = *cur->data;
         // Make sure prev is the predecessor of cur,
         // so that cur_data is correct.
         if (prev->next.load(std::memory_order_acquire) != cur) 
            goto try_again;

         // Can not get cur_data after above invocation,
         // because prev may not be the predecessor of cur at this point.
         if (GreaterOrEquals(cur_data, data)) {
            *prev_ptr = prev;
            *cur_ptr = cur;
            return Equals(cur_data, data);
         }

         // Swap cur_hp and prev_hp.
         HazardPointer tmp = std::move(cur_hp);
         cur_hp = std::move(prev_hp);
         prev_hp = std::move(tmp);

         prev = cur;
         cur = next;
      }
   };

   assert(false);
   return false;
}

template<typename T, typename cmp>
void LockFreeLinkedList<T,cmp>::show(std::ostream& ous) const {
   ous << "size=" << size() << std::endl; 
   Node* x = head_;
   if (x == nullptr) {
      ous << "empty list\n";
      return;
   }
   // following line got segmentation fault
   ous << "head data is " << *(x->data) << std::endl;
   /*
   while (x != nullptr) {
      ous << *(x->data) << ", ";
      //x = x->next.load(std::memory_order_acquire);
      x = x->next.load();
   }
   */
   ous << std::endl;
}

#endif  // LOCKFREE_LINKEDLIST_H
