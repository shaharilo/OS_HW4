//
// Created by eitan on 25/06/2023.
//

/*Tries to allocate ‘size’ bytes.
● Return value:
i. Success –a pointer to the first allocated byte within the allocated block.
 ii. Failure –
a. If ‘size’ is 0 returns NULL.
b. If ‘size’ is more than 108, return NULL.
c. If sbrk fails, return NULL. */
#include <stdio.h>
#include <unistd.h>
#define MAX_SIZE 100000000


void* smalloc(size_t size)
{
    if (0 == size)
    {
        return nullptr;
    }
    if(size > MAX_SIZE)
    {
        return nullptr;
    }
    void* pointer_to_data = nullptr;
    pointer_to_data = sbrk(size);
    if ((void*)(-1) == pointer_to_data  )
    {
        return nullptr;
    }
    return pointer_to_data;
}