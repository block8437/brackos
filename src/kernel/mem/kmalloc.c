#include <mem/early.h>
#include <mem/kmalloc.h>
#include <mem/large.h>
#include <mem/slab.h>
#include <mem/slub.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <kprint.h>

uintptr_t kernel_mem_end;
bool kern_mem_early = true;

// Kernel malloc
void* kmalloc(unsigned long count) {
    if ( kern_mem_early ) {
        return early_kmalloc(count);
    }
    else if ( count <= 4096 ) {
        return _kmalloc(count);
    }
    else {
        return large_mem_alloc(count);
    }
}

// Kernel calloc
void* kcalloc(unsigned long count, unsigned long size) {
    void* ptr = kmalloc(count * size);

    if ( ptr == NULL ) {
        return NULL;
    }

    memset(ptr, 0, count * size);

    return ptr;
}

// Kernel realloc
void* krealloc(void* addr, unsigned long size) {
    if ( addr == NULL ) {
        return kmalloc(size);
    }
    
    // Early kernel memory can't be realloc
    if ( kern_mem_early && (uintptr_t) addr < kernel_mem_end ) {
        return NULL;
    }
    else {
        return _krealloc(addr, size);
    }
}

// Kernel free
void kfree(void* addr) {
    // Early kernel memory is forever :)
    if ( kern_mem_early && (uintptr_t) addr < kernel_mem_end ) {
        return;
    }
    else if ( !_kfree(addr) && !large_mem_dealloc(addr) ) {
        // Neither allocators had it, panic!
    }
}

void kmem_swap() {
    void* end = early_kmalloc_end();
    kprintf("kmalloc: Ended early kernel mem at %p\n", end);
    kernel_mem_end = (uintptr_t) end;
    kern_mem_early = false;
}
