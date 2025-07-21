#define DEBUG
#include "mmm_arena.h"

int main(void) {

    arena *a1 = new_arena(2000);
    // arena *a2 = new_arena(2000);
    // arena *a3 = new_arena(2000);
    // arena_free(a2);
    // arena *a4 = new_arena(2000);
    // arena *a5 = new_arena(2000); 
    // arena *a6 = new_arena(2000);
    // arena_free(a3);
    // arena *a7 = new_arena(2000);
    // arena *a8 = new_arena(2000);
    void *v1 = a_malloc(1000, a1);
    *((int *)v1) = 111;
    arena_register_ptr(a1, &v1);
    void *v2 = a_malloc(500, a1);
    *((int *)v2) = 2222;
    arena_register_ptr(a1, &v2);
    void *v3 = a_malloc(300, a1);
    *((int *)v3) = 333333;
    arena_register_ptr(a1, &v3);
    void *v4 = a_calloc(100, a1);
    *((int *)v4) = 44444444;
    arena_register_ptr(a1, &v4);
    void *v5 = a_malloc(105, a1);
    *((int *)v5) = 555555;
    arena_register_ptr(a1, &v5);
    void *v6 = a_malloc(95, a1);
    *((int *)v6) = 666999666;
    arena_register_ptr(a1, &v6);
    void *v7 = a_malloc(7, a1);
    *((int *)v7) = 7777777;
    arena_register_ptr(a1, &v7);
    void *v8 = a_malloc(32000, a1);
    printf("Numbers: %d %d %d %d %d %d %d\n", *((int *)v1), *((int *)v2), *((int *)v3), *((int *)v4), *((int *)v5), *((int *)v6), *((int *)v7));
    arena *cpy = arena_copy(a1);
    
    puts("Done!");
}