//
// Created by eitan on 25/06/2023.
//

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <cstdint>
#include <string.h>
#include <iostream>
#include <cmath>
#include <sys/mman.h>


#define INITIAL_SLOT_SIZE 128 * 1024
#define INITIAL_SLOT_COUNT 32
#define BLOCK_SIZE_PATTERN 128
#define MAX_ORDER 10
#define ERROR -1
#define MAX_SIZE 128 * 1024
#define MAX_VAL_TO_ALLOC (int)1e8


//------------------------------ general functions ---------------------------------------
void* init_total_memory() {
    void * heap_start = sbrk(0);
    if ((void *) (-1) == heap_start) {
        return nullptr;
    }

    size_t heap_start_ptr = (size_t)heap_start;
    size_t align_size = (MAX_SIZE * INITIAL_SLOT_COUNT) + MAX_SIZE- heap_start_ptr%(MAX_SIZE);
    void* heap_alignment = sbrk(align_size);
    if ((void *) (-1) == heap_alignment) {
        return nullptr;
    }

    size_t heap_array = INITIAL_SLOT_COUNT * INITIAL_SLOT_SIZE;
    void *ptr_total_memory= sbrk(heap_array);
    if ((void *) (-1) == ptr_total_memory) {
        return nullptr;
    }
    // we return non nullptr cause init succeed
    return ptr_total_memory;
}

int getClosetSlotInOrderArray(size_t n) {
    int i=0;
    while (BLOCK_SIZE_PATTERN* std::pow(2,i) < n) {
        i++;
    }
    if (i > MAX_ORDER)
    {
        return ERROR;
    }
    return i;
}


int32_t cookie_val = rand();
//--------------------------------- meta data node --------------------------------
class MallocMetadataNode {
public:
    int32_t cookie;
    size_t data_size; //how much data was allocated in the block
    bool is_free;
    MallocMetadataNode *next;
    MallocMetadataNode *prev;


    ////methods:
    void initNode(size_t size);

    int getCookie();
    size_t getDataSize();
    bool getIsFree();
    MallocMetadataNode* getNext();
    MallocMetadataNode* getPrev();

    void setCookie(int new_cookie);
    void setDataSize(size_t new_size);
    void setIsFree(bool new_is_free);
    void setNext(MallocMetadataNode* new_node);
    void setPrev(MallocMetadataNode* new_node);

};



void MallocMetadataNode::initNode(size_t size){
    this->cookie = cookie_val;
    this->data_size = size;
    this->is_free =false;
    this->next = nullptr;
    this->prev = nullptr;
}

int MallocMetadataNode::getCookie() {
    return this->cookie;
}

void MallocMetadataNode::setCookie(int32_t new_cookie) {
    this->cookie = new_cookie;
}

void MallocMetadataNode::setDataSize(size_t new_size) {
    if(this->getCookie() != cookie_val){
        exit(0xdeadbeef);
    }
    this->data_size = new_size;
}

size_t MallocMetadataNode::getDataSize() {
    return this->data_size;
}

bool MallocMetadataNode::getIsFree() {
    return this->is_free;
}

MallocMetadataNode *MallocMetadataNode::getNext() {
    return this->next;
}

MallocMetadataNode *MallocMetadataNode::getPrev() {
    return this->prev;
}

void MallocMetadataNode::setIsFree(bool new_is_free) {
    if(this->getCookie() != cookie_val){
        exit(0xdeadbeef);
    }
    this->is_free = new_is_free;
}

void MallocMetadataNode::setNext(MallocMetadataNode *new_node) {
    if(this->getCookie() != cookie_val){
        exit(0xdeadbeef);
    }
    this->next = new_node;
}

void MallocMetadataNode::setPrev(MallocMetadataNode *new_node) {
    if(this->getCookie() != cookie_val){
        exit(0xdeadbeef);
    }
    this->prev = new_node;
}

size_t meta_data_size = sizeof(MallocMetadataNode);

MallocMetadataNode* get_actual_meta_data(void* data){
    if(((MallocMetadataNode *)((char*)data-meta_data_size))->getCookie() != cookie_val)
    {
        exit(0xdeadbeef);
    }
    return (MallocMetadataNode *)((char *)data-meta_data_size);
}

//------------------------------------------- meta data list ---------------------------------------------

class MallocMetadataList{
public:
    MallocMetadataNode* head;
    MallocMetadataNode* tail;
    size_t list_size;


    //methods:
    MallocMetadataList();
    void insertByAddress(MallocMetadataNode* node_to_insert, bool is_with_stats);
    void* allocateWithMmap(size_t size_of_block);
    void freeWithMunmap(void* data);
    void* remove_head();
    void* remove_head_without_stats();
    void* removeByAddress(MallocMetadataNode* block_to_remove);
    void* findByAddress(MallocMetadataNode* block_to_find);
    void display();

};

//----------------------------------------------------order array - holds the free blocks -----------------------------------------------
class OrderArray{
public:
    MallocMetadataList* order_array_of_free_blocks[11];
    size_t num_of_free_blocks;
    size_t num_of_free_bytes;
    size_t num_of_allocated_blocks;
    size_t num_of_allocated_bytes;

    OrderArray();
    bool insertFreeBlock(MallocMetadataNode* block_to_insert);
    void* removeFreeBlock(size_t size);

    bool splitBlock(MallocMetadataNode* node_to_split,size_t requested_size, int order);
    bool mergeBlocks(MallocMetadataNode* block_to_insert, size_t size, int order,MallocMetadataNode** address_of_merged_block );
    bool sreallocMergeBlocks(MallocMetadataNode* block_to_insert, size_t size, int order,MallocMetadataNode* address_of_merged_block);
    bool sreallocInsertFreeBlock(MallocMetadataNode* block_to_insert);

};
//---------------------------- order array functions implementations ------------------------------------
OrderArray::OrderArray()
        : num_of_free_blocks(0),
          num_of_free_bytes(0),
          num_of_allocated_blocks(0),
          num_of_allocated_bytes(0)
{
    for (int i = 0; i < 11; i++)
    {
        order_array_of_free_blocks[i] = new MallocMetadataList();
    }
}

OrderArray* order_array = new OrderArray();


bool OrderArray::insertFreeBlock(MallocMetadataNode* block_to_insert)
{
    std::cerr << "------- inside insert free block-----------------" << std::endl;
    int i = getClosetSlotInOrderArray(block_to_insert->getDataSize() + meta_data_size);
    std::cerr << " i = "<<i << std::endl;
    if (i != ERROR)
    {
        std::cerr << "------- inside i!=ERROR -----------------, i = "<<i << std::endl;
        size_t size = block_to_insert->getDataSize();
        MallocMetadataNode* address_of_merged_block = nullptr;

        std::cerr << "before the merge" << std::endl;
        //add the first block that we now want to free
        order_array_of_free_blocks[i]->insertByAddress(block_to_insert, true);

        while (mergeBlocks(block_to_insert, size, i, &address_of_merged_block))
        {
            std::cerr << "----------------inside merge loop--------------"<<std::endl;
            i++;
            size*=2;
            block_to_insert = address_of_merged_block;
        }
        std::cerr << "----------------after merge loop--------------"<<std::endl;
        std::cerr << "allocated blocks after merge = " << num_of_allocated_blocks<<std::endl;
        std::cerr << "free blocks after merge = " << num_of_free_blocks<<std::endl;
        return true;
    }
    return false;
}

void* OrderArray::removeFreeBlock(size_t size) {
    int i = getClosetSlotInOrderArray(size);
    std::cerr << "---------------index inside removeFreeBlock:" << i <<std::endl;

    if (i != ERROR)
    {
        while(i<11 && order_array_of_free_blocks[i]->head == nullptr){
            i++;
        }
        std::cerr << "---------------index after while:" << i <<std::endl;
        if(i != 11){
            std::cerr << "free bytes before remove head:" << num_of_free_bytes <<std::endl;
            void* remove_result = order_array_of_free_blocks[i]->remove_head();
            std::cerr << "free bytes after remove head:" << num_of_free_bytes <<std::endl;
            while (splitBlock((MallocMetadataNode *) remove_result, size, i))
            {
                std::cerr << "---------------index inside split loop:" << i <<std::endl;
                i--;
                remove_result = order_array_of_free_blocks[i]->remove_head();
            }

            if(remove_result!= nullptr){
                MallocMetadataNode* result = (MallocMetadataNode*)remove_result;
                result->setIsFree(false);
                result->setCookie(cookie_val);
            }
            return remove_result;
        }
    }
    return nullptr;
}

void* findBuddyAddress(MallocMetadataNode* block, size_t size, int order){
    size_t actual_size = size;
    std::cerr << "block address for XOR " << block<< "       actual size for XOR = "<< actual_size <<std::endl;
    MallocMetadataNode* address = (MallocMetadataNode*)(((size_t)(block)) ^ actual_size);
    std::cerr << "XOR result " << address<<std::endl;
    void* buddy = order_array->order_array_of_free_blocks[order]->findByAddress(address);

    return buddy;


}

bool OrderArray::splitBlock(MallocMetadataNode *block_to_alloc, size_t requested_size,int order) {
    //here we already know we need to do at least one split
    std::cerr << "---------------inside split function-------------:" <<std::endl;
    int curr_order = order;
    MallocMetadataNode* node_to_split = block_to_alloc;
    if(curr_order != 0 && std::pow(2,curr_order)*BLOCK_SIZE_PATTERN >= 2*(requested_size)) {
        //continue splitting

        std::cerr << "order = " << order<< " if condition in split func = "<<std::pow(2,curr_order)*BLOCK_SIZE_PATTERN <<std::endl;
        curr_order--;
        size_t size_of_splitted_block = std::pow(2,curr_order)*BLOCK_SIZE_PATTERN;
        node_to_split->setDataSize(size_of_splitted_block);
        order_array_of_free_blocks[curr_order]->insertByAddress(node_to_split, true);
        MallocMetadataNode* right_half = reinterpret_cast<MallocMetadataNode*>(reinterpret_cast<char*>(node_to_split) + (int)size_of_splitted_block);
        std::cerr <<"left half = "<<node_to_split<<" right half = " << right_half<<std::endl;
        right_half->setCookie(cookie_val);
        right_half->setDataSize(size_of_splitted_block);
        order_array_of_free_blocks[curr_order]->insertByAddress(right_half, true);

        return true;
    }
    return false;
}

bool OrderArray::mergeBlocks(MallocMetadataNode* block_to_insert, size_t size, int order, MallocMetadataNode** address_of_merged_block) {
    if(order == 10){
        return false;
    }
    std::cerr << "order = " << order<<std::endl;

    size_t size_of_block = std::pow(2,order)*BLOCK_SIZE_PATTERN;

    MallocMetadataNode* buddy = (MallocMetadataNode*)findBuddyAddress(block_to_insert,size_of_block, order);
    std::cerr << "----------------after findBuddy--------------"<<std::endl;
    if(buddy == nullptr || !buddy->getIsFree()){
        if(buddy == nullptr){
            std::cerr << "----------------buddy is nullptr--------------"<<std::endl;
        }
        std::cerr << "----------------no buddy--------------"<<std::endl;
        block_to_insert->setIsFree(true);
        *address_of_merged_block = block_to_insert;

        return false;
    }

    //buddy exists and free
    std::cerr << "----------------found buddy--------------"<<std::endl;
    std::cerr << "allocated blocks before removes = " << num_of_allocated_blocks<<std::endl;
    std::cerr << "free blocks before removes = " << num_of_free_blocks<<std::endl;
    std::cerr << "address of block to insert before removes = " << block_to_insert<<std::endl;
    order_array_of_free_blocks[order]->removeByAddress(block_to_insert);
    order_array_of_free_blocks[order]->removeByAddress(buddy);
    std::cerr << "allocated blocks after removes = " << num_of_allocated_blocks<<std::endl;
    std::cerr << "free blocks after removes = " << num_of_free_blocks<<std::endl;
    order++;
    size_t new_size =size_of_block*2;
    bool is_buddy_smaller = false;
    if(buddy < block_to_insert){
        is_buddy_smaller = true;
    }
    if(is_buddy_smaller){
        buddy->setDataSize(new_size);
        buddy->setIsFree(true);
        *address_of_merged_block = buddy;
        std::cerr << "merged address - buddy smaller = "<< address_of_merged_block<<std::endl;
        order_array_of_free_blocks[order]->insertByAddress(*address_of_merged_block, false);
    }
    else{
        block_to_insert->setDataSize(new_size);
        block_to_insert->setIsFree(true);
        *address_of_merged_block = block_to_insert;
        std::cerr << "merged address - original block smaller = "<< *address_of_merged_block<<std::endl;
        order_array_of_free_blocks[order]->insertByAddress(*address_of_merged_block, false);
    }
    std::cerr << "allocated blocks after insert = " << num_of_allocated_blocks<<std::endl;
    std::cerr << "free blocks after insert = " << num_of_free_blocks<<std::endl;
    return true;


}

bool OrderArray::sreallocMergeBlocks(MallocMetadataNode* block_to_insert, size_t size, int order,MallocMetadataNode* address_of_merged_block) {
    if (order == 10) {
        return false;
    }
    MallocMetadataNode *buddy = (MallocMetadataNode *)findBuddyAddress(block_to_insert, size, order);
    if (buddy == nullptr || !buddy->getIsFree()) {
        return false;
    }
    bool is_buddy_smaller = false;
    if (&buddy < &block_to_insert) {
        is_buddy_smaller = true;
    }
    if (is_buddy_smaller) {
        address_of_merged_block = buddy;

    } else {
        address_of_merged_block = block_to_insert;
    }
    return true;
}

int srealloc_shouldMerge(MallocMetadataNode* block_to_insert, size_t size, int order,MallocMetadataNode* address_of_merged_block) {
    size_t curr_size_off_merged = block_to_insert->getDataSize();
    int times= 0;
    while (curr_size_off_merged < size  && order_array->sreallocMergeBlocks(block_to_insert,  size,  order, address_of_merged_block))
    {
        curr_size_off_merged *= 2;
        order++;
        block_to_insert = address_of_merged_block;
        times ++;
    }
    return times;
}


bool OrderArray::sreallocInsertFreeBlock(MallocMetadataNode* block_to_insert)
{
    {
        int i = getClosetSlotInOrderArray(block_to_insert->getDataSize() + meta_data_size);
        if (i != ERROR)
        {
            size_t size = block_to_insert->getDataSize();
            MallocMetadataNode* address_of_merged_block = nullptr;
            int times = srealloc_shouldMerge(block_to_insert, size, i,address_of_merged_block);

            for (int i = 0; i< times; i++)
            {
                mergeBlocks(block_to_insert, size, i, &address_of_merged_block);
                i++;
                size*=2;
                block_to_insert = address_of_merged_block;
            }
            num_of_allocated_bytes-= block_to_insert->getDataSize();
            if (num_of_allocated_blocks != 0){
                num_of_allocated_blocks--;
            }
            num_of_free_bytes+=block_to_insert->getDataSize();
            num_of_free_blocks++;
            return true;
        }
        return false;
    }
}


//---------------------------- meta data list functions implementations ------------------------------------

MallocMetadataList::MallocMetadataList() : head(nullptr),tail(nullptr), list_size(0){}


void MallocMetadataList::insertByAddress(MallocMetadataNode* node_to_insert, bool is_with_stats) {
    //std::cerr << "inside insert" << std::endl;
    if(node_to_insert == nullptr)
    {
        std::cerr << "ERROR" << std::endl;
        return;
    }
    if (this->head == nullptr) {
        // If the list is empty, insert the new node as the head
        //std::cerr << "head of list is null" << std::endl;
        this->head = node_to_insert;
        this->tail = node_to_insert;
        node_to_insert->setNext(nullptr);
        node_to_insert->setPrev(nullptr);

    }
    else {
        MallocMetadataNode *current = this->head;
        if(current > node_to_insert){
            //insert node as new head
            MallocMetadataNode *head_node = this->head;
            head_node->setPrev(node_to_insert);
            node_to_insert->setNext(head_node);
            this->head = node_to_insert;
            node_to_insert->setPrev(nullptr);

        }
        else{
            while (current != nullptr && current < node_to_insert) {
                current = current->next;
            }

            if (current != nullptr) {
                current = current->prev;
                node_to_insert->setPrev(current) ;
                node_to_insert->setNext(current->next);
                if (current->getNext() != nullptr) {
                    current->getNext()->setPrev(node_to_insert);
                }
                current->setNext(node_to_insert);
            } else {
                //std::cerr << "insert as tail" << std::endl;
                MallocMetadataNode *tail_node = this->tail;
                tail_node->setNext(node_to_insert);
                node_to_insert->setPrev(tail_node);
                this->tail = node_to_insert;
                node_to_insert->setNext(nullptr);
            }
        }
    }
    int index = getClosetSlotInOrderArray(node_to_insert->getDataSize());
    size_t actual_size = BLOCK_SIZE_PATTERN * std::pow(2,index);


    order_array->num_of_free_blocks++;
    order_array->num_of_free_bytes+= actual_size;

    /*if(is_with_stats){
        if (order_array->num_of_allocated_blocks != 0){
            order_array->num_of_allocated_blocks--;
        }
        if (order_array->num_of_allocated_bytes != 0){
            order_array->num_of_allocated_bytes-=actual_size;
        }
    }*/
    node_to_insert->setIsFree(true);
    std::cerr << "allocated blocks after insert_by_address = " << order_array->num_of_allocated_blocks<<std::endl;
    std::cerr << "free blocks after insert_by_address = " << order_array->num_of_free_blocks<<std::endl;


}




void* MallocMetadataList::allocateWithMmap(size_t size_of_block) {
    void* new_block = mmap(nullptr, size_of_block+meta_data_size, PROT_READ | PROT_WRITE, MAP_PRIVATE |
                                                                                          MAP_ANONYMOUS, -1, 0);
    if(new_block == MAP_FAILED){
        return nullptr;
    }
    MallocMetadataNode* mmap_node = (MallocMetadataNode*)new_block;
    mmap_node->setCookie(cookie_val);
    mmap_node->setDataSize(size_of_block);
    mmap_node->setIsFree(false);
    order_array->num_of_allocated_blocks++;
    order_array->num_of_allocated_bytes+=size_of_block + meta_data_size;
    return (void*)((char*)(new_block) + meta_data_size);
}

void MallocMetadataList::freeWithMunmap(void *data) {
    MallocMetadataNode* block = get_actual_meta_data(data);
    size_t curr_block_size = block->getDataSize();
    order_array->num_of_allocated_blocks--;
    order_array->num_of_allocated_bytes-=(curr_block_size+meta_data_size);
    munmap((void*)block, meta_data_size + curr_block_size);

}


void* MallocMetadataList::remove_head() {
    std::cerr << "--------inside remove head----------------." << std::endl;
    MallocMetadataNode* list_head = this->head;

    if (list_head != nullptr)
    {
        std::cerr << "--------list head is not null------------." << std::endl;
        int index = getClosetSlotInOrderArray(list_head->getDataSize());
        size_t actual_size = BLOCK_SIZE_PATTERN * std::pow(2,index);
        this->head = list_head->next;
        if(this->head != nullptr){
            this->head->setPrev(nullptr);
        }

        if (order_array->num_of_free_blocks != 0){
            order_array->num_of_free_blocks--;
        }
        if (order_array->num_of_free_bytes != 0){
            order_array->num_of_free_bytes-= actual_size;
        }
        /*order_array->num_of_allocated_blocks++;
        order_array->num_of_allocated_bytes+=actual_size;*/
        std::cerr << "--------remove head - before the return------------." << std::endl;
        std::cerr << "allocated blocks after remove head = " << order_array->num_of_allocated_blocks<<std::endl;
        std::cerr << "free blocks after remove head = " << order_array->num_of_free_blocks<<std::endl;

        return list_head;
    }
    std::cerr << "Node not found in the linked list." << std::endl;
    return nullptr;
}

void* MallocMetadataList::remove_head_without_stats() {
    std::cerr << "--------inside remove head----------------." << std::endl;
    MallocMetadataNode* list_head = this->head;

    if (list_head != nullptr)
    {
        std::cerr << "--------list head is not null------------." << std::endl;
        int index = getClosetSlotInOrderArray(list_head->getDataSize());
        size_t actual_size = BLOCK_SIZE_PATTERN * std::pow(2,index);
        this->head = list_head->next;
        if(this->head != nullptr){
            this->head->setPrev(nullptr);
        }

        if (order_array->num_of_free_blocks != 0){
            order_array->num_of_free_blocks--;
        }
        if (order_array->num_of_free_bytes != 0){
            order_array->num_of_free_bytes-= actual_size;
        }
        std::cerr << "--------remove head - before the return------------." << std::endl;
        std::cerr << "allocated blocks after remove head = " << order_array->num_of_allocated_blocks<<std::endl;
        std::cerr << "free blocks after remove head = " << order_array->num_of_free_blocks<<std::endl;

        return list_head;
    }
    std::cerr << "Node not found in the linked list." << std::endl;
    return nullptr;
}


void *MallocMetadataList::removeByAddress(MallocMetadataNode *block_to_remove) {
    if(head == nullptr || block_to_remove == nullptr){
        return nullptr;
    }
    MallocMetadataNode* curr_node = head;
    while(curr_node!= nullptr && curr_node!=block_to_remove){
        curr_node = curr_node->getNext();
    }
    std::cerr<<"****************PART 1********************"<<std::endl;
    if(curr_node == block_to_remove){
        int order = getClosetSlotInOrderArray(curr_node->getDataSize());
        size_t size_of_block = std::pow(2,order)*BLOCK_SIZE_PATTERN;
        if(curr_node == head){
            std::cerr<<"****************PART 2********************"<<std::endl;
            return remove_head_without_stats();
        }
        //else
        std::cerr<<"****************PART 3********************"<<std::endl;
        if(curr_node->getPrev()!=nullptr){
            curr_node->getPrev()->setNext(curr_node->getNext());
        }
        if( curr_node->getNext()!=nullptr){
            curr_node->getNext()->setPrev(curr_node->getPrev());
        }
        //delete curr_node;
        std::cerr<<"****************PART 4********************"<<std::endl;
        if (order_array->num_of_free_blocks != 0 )
        {
            order_array->num_of_free_blocks--;
        }

        order_array->num_of_free_bytes-= size_of_block;
        return curr_node;
    }
    return nullptr;
}

void *MallocMetadataList::findByAddress(MallocMetadataNode *block_to_find) {
    if(this->head == nullptr){
        std::cout <<" head is null:" << std::endl;
        return nullptr;
    }
    MallocMetadataNode* curr_node = this->head;
    std::cout <<" block to find address:" << block_to_find << std::endl;
    while(curr_node!= nullptr){
        std::cout <<" curr node = " << curr_node << std::endl;
        if(curr_node == block_to_find) {
            return curr_node;
        }
        curr_node = curr_node->getNext();
    }

    return nullptr;
}


void MallocMetadataList::display() {
    MallocMetadataNode* current = head;
    std::cerr << "START!" << std::endl;

    while (current != nullptr) {
        std::cerr << "Node Address: " << current << std::endl;
        std::cerr << "Data Size: " << current->getDataSize() << std::endl;
        std::cerr << "Is Free: " << (current->getIsFree() ? "Yes" : "No") << std::endl;
        std::cerr << "Next Node Address: " << current->getNext() << std::endl;
        std::cerr << "Previous Node Address: " << current->getPrev() << std::endl;
        std::cerr << "-----------------------" << std::endl;
        current = current->getNext();
    }
    std::cerr << "END!" << std::endl;

}



MallocMetadataList* mmap_blocks_list = new MallocMetadataList();





//------------------------- final functions -------------------------------------
bool first_malloc_occured = false;

void* smalloc(size_t size){

    std::cerr << "Start OF SMALLOCCCCCCCCCCCCCCCCC"  <<std::endl;
    std::cerr << "requested size: "<< size <<std::endl;
    if (first_malloc_occured == false)
    {
        //order_array = new OrderArray();
        void* ptr = init_total_memory();
        if (ptr != nullptr)
        {
            for (int i = 0; i < 32; i++)
            {
                MallocMetadataNode* node = (MallocMetadataNode*)((size_t)ptr + i*INITIAL_SLOT_SIZE);

                node->initNode(INITIAL_SLOT_SIZE);
                (order_array->order_array_of_free_blocks[10])->insertByAddress(node, true);
            }
        }
        order_array->num_of_free_blocks = INITIAL_SLOT_COUNT;
        order_array->num_of_free_bytes =  INITIAL_SLOT_SIZE * INITIAL_SLOT_COUNT;
        order_array->num_of_allocated_blocks =(0);
        order_array->num_of_allocated_bytes =(0);
        first_malloc_occured = true;

    }
    void* alloc;
    if(size==0 || size>=MAX_VAL_TO_ALLOC){
        return nullptr;
    }
    if(size>=MAX_SIZE){
        std::cerr << "---------------size>=MAX_SIZE-----------:" <<std::endl;
        alloc = mmap_blocks_list->allocateWithMmap(size);
        return alloc;
    }
    //std::cerr << "meta data size = " << meta_data_size <<std::endl;
    int index = getClosetSlotInOrderArray(size+meta_data_size);
    size_t actual_size = BLOCK_SIZE_PATTERN * std::pow(2,index);
    if(actual_size==0){
        return nullptr;
    }
    alloc = order_array->removeFreeBlock(actual_size);
    if((MallocMetadataNode*)alloc == nullptr){
        return nullptr;
    }
    order_array->num_of_allocated_blocks++;
    order_array->num_of_allocated_bytes+=actual_size;
    std::cerr<<"actual size in free = "<<actual_size<< std::endl;
    std::cerr << "the address is:" << alloc<<std::endl;
    ((MallocMetadataNode*)(alloc))->initNode(size);
    std::cerr << "END OF SMALLOCCCCCCCCCCCCCCCCC,     the address is:" << alloc<<std::endl;
    return (MallocMetadataNode *)((char *)alloc+meta_data_size); //will return null in the case of failure in alloc
}

void sfree(void* p){
    std::cerr<<"!!!!!!!!!!!!!!!!!!!!!HERE!!!!!!!!!!!!"<< std::endl;
    if(p == nullptr){
        std::cerr<<"error in free"<< std::endl;
        return;
    }
    MallocMetadataNode* node = (MallocMetadataNode*)p;
    size_t original_data_size = get_actual_meta_data(p)->getDataSize() + meta_data_size;

    if(get_actual_meta_data(node)->getIsFree()){
        std::cerr<<"error in free"<< std::endl;
        return;
    }
    std::cerr<<"!!!!!!!!!!!!!!!!!!!!!THERE!!!!!!!!!!!!"<< std::endl;
    if(get_actual_meta_data(p)->getDataSize() + meta_data_size>= MAX_SIZE){
        //it was allocated with mmap
        std::cerr<<"big block in free"<< std::endl;
        mmap_blocks_list->freeWithMunmap(p);
        return;
    }
    std::cerr<<"small block in free"<< std::endl;
    order_array->insertFreeBlock(get_actual_meta_data(p));

    std::cerr<<"size for calc index = "<<original_data_size<< std::endl;
    int index = getClosetSlotInOrderArray(original_data_size);
    size_t actual_size = BLOCK_SIZE_PATTERN * std::pow(2,index);
    std::cerr<<"index in free = "<<index<< std::endl;
    std::cerr<<"actual size in free = "<<actual_size<< std::endl;
    order_array->num_of_allocated_blocks--;
    order_array->num_of_allocated_bytes-=actual_size;

}


void* scalloc(size_t num, size_t size){
    if(size==0|| size>=MAX_VAL_TO_ALLOC){
        return nullptr;
    }
    size_t total_size = size*num;
    void* alloc = smalloc(total_size);
    if(alloc == nullptr){
        return alloc;
    }
    memset(alloc, 0, total_size);
    return alloc;
}

size_t _num_free_blocks();
size_t _num_free_bytes();
size_t _num_allocated_blocks();
size_t _num_allocated_bytes();
size_t _num_meta_data_bytes();
size_t _size_meta_data();



size_t _num_free_blocks(){
    return order_array->num_of_free_blocks;
}
size_t _num_free_bytes(){
    return order_array->num_of_free_bytes-(order_array->num_of_free_blocks * meta_data_size);
}
size_t _num_allocated_blocks(){
    return order_array->num_of_allocated_blocks+ order_array->num_of_free_blocks;
}
size_t _num_allocated_bytes(){
    return order_array->num_of_allocated_bytes + order_array->num_of_free_bytes
           - (order_array->num_of_allocated_blocks * meta_data_size) - (order_array->num_of_free_blocks)*meta_data_size;
}
size_t _num_meta_data_bytes(){
    return (order_array->num_of_allocated_blocks + order_array->num_of_free_blocks) * meta_data_size;
}
size_t _size_meta_data(){
    return meta_data_size;
}


void* srealloc(void* oldp, size_t size){
    if(size==0 || size>=MAX_VAL_TO_ALLOC){
        return nullptr;
    }
    if(oldp == nullptr){
        return smalloc(size);
    }
    MallocMetadataNode* old_node = (MallocMetadataNode*)((char*)oldp - meta_data_size);
    size_t old_node_data_size = old_node->getDataSize();

    //mmap
    if((size<MAX_SIZE && old_node_data_size >= MAX_SIZE) || (size>=MAX_SIZE && old_node_data_size < MAX_SIZE)){
        return nullptr;
    }
    if(old_node_data_size >=MAX_SIZE){
        if(old_node_data_size == size){
            return oldp;
        }
        //else
        void* mmap_block = smalloc(size);
        if(old_node_data_size>= size){
            memmove(mmap_block,(void*)((char*)old_node + meta_data_size), size);
        }
        else{
            memmove(mmap_block, (void*)((char*)old_node+meta_data_size),old_node_data_size);
        }
        sfree(oldp);
        return mmap_block;
    }

    //not mmap

    //part a:
    if(size<=old_node->data_size){
        return oldp;
    }

    void* new_alloc_data;

    //part b:
    int order = getClosetSlotInOrderArray(old_node->getDataSize() + meta_data_size);
    MallocMetadataNode* address_of_merged_block = nullptr;
    int times = srealloc_shouldMerge(old_node, size, order, address_of_merged_block);
    if (times > 0)
    {
        order_array->sreallocInsertFreeBlock( old_node);
    }
    /*check if merging buddies (of oldp) will help us with the current allocation:
     * if merge will help us - do the merge with old p and allocate
     * if merge won't help - go to part c
     */

    //part c
    //find an already existing block which is big enough and allocate
    new_alloc_data = smalloc(size);
    if(new_alloc_data == nullptr){
        return new_alloc_data;
    }
    memmove(new_alloc_data,oldp,old_node->data_size);
    sfree(oldp);
    return new_alloc_data;

}





