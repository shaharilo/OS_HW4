//
// Created by eitan on 25/06/2023.
//
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <iostream>

#define MAX_SIZE 100000000
class MallocMetadataNode {
public:
    size_t data_size;
    bool is_free;
    MallocMetadataNode* next;
    MallocMetadataNode* prev;

    //methods:
    void initNode(size_t size);
};

void MallocMetadataNode::initNode(size_t size){
    this->data_size = size;
    this->is_free =false;
    this->next = nullptr;
    this->prev = nullptr;
}


class MallocMetadataList{
public:
    MallocMetadataNode* head;
    MallocMetadataNode* tail;
    size_t list_size;
    size_t num_of_free_blocks;
    size_t num_of_free_bytes;
    size_t num_of_allocated_blocks;
    size_t num_of_allocated_bytes;

    //methods:
    MallocMetadataList();
    void* insertBlockInTail(size_t size_of_block);
    void* findFreeSpace(size_t size_of_block);
    void* insertBlockInFreeSpace(size_t size_of_block);
    void removeBlock(void* block_to_remove);

};
size_t meta_data_size = sizeof(MallocMetadataNode);

MallocMetadataList::MallocMetadataList() : head(nullptr), tail(nullptr), list_size(0), num_of_free_blocks(0), num_of_free_bytes(0), num_of_allocated_blocks(0), num_of_allocated_bytes(0){}

void* MallocMetadataList::insertBlockInTail(size_t size_of_block){
    void* block = sbrk(size_of_block + meta_data_size);
    if(block == (void*)-1){
        return nullptr;
    }
    MallocMetadataNode* new_node = (MallocMetadataNode*)block;
    new_node->initNode(size_of_block);

    if(this->tail == nullptr){
        //meaning - list is empty
        this->tail = new_node;
        this->head = new_node;
    }
    else{
        //list is not empty
        MallocMetadataNode* tail_node = this->tail;
        tail_node->next = new_node;
        new_node->prev = tail_node;
        this->tail = new_node;
    }

    this->list_size ++;
    this->num_of_allocated_blocks++;
    this->num_of_allocated_bytes+=size_of_block;
    return (void*)((char*)block+meta_data_size);
}

void* MallocMetadataList::findFreeSpace(size_t size_of_block){
    MallocMetadataNode* curr_node = this->head;
    while(curr_node!= nullptr){
        if(curr_node->is_free == true && curr_node->data_size>=size_of_block){
            return (void*)curr_node;
        }
        curr_node = curr_node->next;
    }
    return nullptr;
}

void* MallocMetadataList::insertBlockInFreeSpace(size_t size_of_block){
    void* free_space = findFreeSpace(size_of_block);
    if(free_space== nullptr){
        //meaning there is no free space in the gaps between the allocated memory
        return insertBlockInTail(size_of_block);
    }
    //else
    MallocMetadataNode* new_node = (MallocMetadataNode*)free_space;
    new_node->is_free = false;
    num_of_free_blocks--;
    num_of_free_bytes-=size_of_block;
    return (void*)((char*)free_space+meta_data_size);
}

void MallocMetadataList::removeBlock(void* block_to_remove){
    MallocMetadataNode* node_to_remove = (MallocMetadataNode*)((char*)block_to_remove - meta_data_size);
    node_to_remove->is_free = true;
    num_of_free_blocks++;
    num_of_free_bytes+=node_to_remove->data_size;
}
MallocMetadataList* list = new MallocMetadataList(); //global ptr to list - as required


void* smalloc(size_t size){
    if(size==0 || size>MAX_SIZE){
        return nullptr;
    }
    void* alloc = list->insertBlockInFreeSpace(size);
    return alloc;
}

void* scalloc(size_t num, size_t size){
    if(size==0 || size>MAX_SIZE || num ==0 || size*num>MAX_SIZE){
        return nullptr;
    }
    size_t total_size = size*num;
    void* alloc = list->insertBlockInFreeSpace(total_size);
    if(alloc == nullptr){
        return alloc;
    }
    memset(alloc, 0, total_size);
    return alloc;
}

void sfree(void* p){
    if(p== nullptr){
        return;
    }
    list->removeBlock(p);
}

void* srealloc(void* oldp, size_t size){
    if(size==0 || size>MAX_SIZE){
        return nullptr;
    }
    if(oldp == nullptr){
        return smalloc(size);
    }
    MallocMetadataNode* old_node = (MallocMetadataNode*)((char*)oldp - meta_data_size);
    if(size<=old_node->data_size){
        return oldp;
    }
    //else
    void* new_alloc_data = smalloc(size);
    if(new_alloc_data == nullptr){
        return new_alloc_data;
    }
    memmove(new_alloc_data,oldp,old_node->data_size);
    sfree(oldp);
    return new_alloc_data;
}

size_t _num_free_blocks(){
    return list->num_of_free_blocks;
}
size_t _num_free_bytes(){
    return list->num_of_free_bytes;
}
size_t _num_allocated_blocks(){
    return list->num_of_allocated_blocks;
}
size_t _num_allocated_bytes(){
    return list->num_of_allocated_bytes;
}
size_t _num_meta_data_bytes(){
    return list->num_of_allocated_blocks * meta_data_size;
}
size_t _size_meta_data(){
    return meta_data_size;
}