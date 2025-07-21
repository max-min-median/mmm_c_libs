#ifndef MMM_HASHMAP
#define MMM_HASHMAP

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "mmm_debug.h"
#include "mmm_arena.h"
#include "mmm_hashset.h"

#define MAP_ERROR ((int64_t) -1)

typedef struct {
    hashset *set;
    arena *values;
    uint32_t *value_offsets;

/*  value_size < 0: values are dynamically sized, and the first `abs(value_size)` bytes indicate the size of the value.
                   For example, if value_size = -2, the first 2 bytes give the size of the value.
    value_size == 0: values are null-terminated strings.
    value_size > 0: values occupy a fixed number of bytes, given by `value_size`
*/
    int32_t value_size; 
} hashmap;

void map_resize(hashmap *map);

hashmap *new_map(uint32_t capacity, int32_t key_size, int32_t value_size, uint32_t (*hash_func)(void *)) {

    hashmap *map = malloc(sizeof(hashmap));
    if (map == NULL) {
        printf("new_map(): Failed to allocate %zu bytes for hashmap struct!\n", sizeof(hashmap));
        return NULL;
    }

    map->value_offsets = calloc(capacity, sizeof(*map->value_offsets));
    if (map->value_offsets == NULL) {
        printf("new_map(): Failed to allocate %zu bytes for value-offsets array!\n", capacity * sizeof(*map->value_offsets));
        free(map);
        return NULL;
    }

    map->set = new_set(capacity, key_size, hash_func);
    if (map->set == NULL) {
        printf("new_map(): Failed to initialize new_set()!");
        free(map->value_offsets);
        free(map);
        return NULL;
    }

    uint32_t arena_size = capacity * (value_size <= 0 ? 256 : value_size);  // default variable arena size is 256 bytes per value
    map->values = new_arena(arena_size);
    if (map->values == NULL) {
        printf("new_map(): Failed to initialize values arena of %zu bytes.\n", arena_size);
        set_free(map->set);
        free(map->value_offsets);
        free(map);
        return NULL;
    }

    a_malloc(1, map->values);  // if set->offsets[hash] == 0, that indicates no key at that hash value.
    return map;
}


/**
 * Adds the key-value pair (item, value)`item` to `set`, checking first if it already exists.
 * Returns the offset of the item in the arena if successfully added, else 0.
 */
int64_t map_put(void *item, void *value, hashmap *map, uint32_t item_size, uint32_t value_size) {

    if (map->value_size >= 0 && map->value_size != value_size) {
        puts("map_put(): Value is not the correct size!");
        return MAP_ERROR;
    }

    int64_t hash = set_add(item, map->set, item_size);
    if (hash == SET_ERROR) {
        puts("map_put(): Failed to add key to set");
        return SET_ERROR;
    }

    if (map->value_size < 0) {  // variable value size
        map->value_offsets[hash] = a_malloc(value_size - map->value_size, map->values) - map->values->ptr;
        if (map->value_offsets[hash] == 0) {
            puts("map_put(): Failed to allocate memory in arena!");
            return MAP_ERROR;
        }
        uint32_t value_size_copy = value_size;
        for (int32_t i = 0; i < -map->value_size; i++) {
            *(uint8_t *)(map->values->ptr + map->value_offsets[hash] + i) = value_size_copy & 0xFF;
            value_size_copy >>= 8;
        }
        memcpy(map->values->ptr + map->value_offsets[hash] - map->value_size, value, value_size);
    } else {
        if (value_size == 0) value_size = strlen(value) + 1;  // null-terminated strings
        map->value_offsets[hash] = a_malloc(value_size, map->values) - map->values->ptr;
        if (map->value_offsets[hash] == 0) {
            puts("map_put(): Failed to allocate memory in arena!");
            return MAP_ERROR;
        }
        memcpy(map->values->ptr + map->value_offsets[hash], value, value_size);
    }
    DEBUG_PRINTF("map_put(): adding entry to hash index %d\n", hash);
    return hash;
}

/**
 * Removes key-value pair from `map` if it exists.
 * Returns the (previous) final hash position of the key if successfully deleted, else -1.
 */
int64_t map_delete(void *item, hashmap *map, uint32_t item_size) {

    if (set_delete(item, map->set, item_size) == SET_ERROR) {
        puts("map_delete(): Key not found!");
        return SET_ERROR;
    }
}

/**
 * Returns a boolean for whether `key` is in `map`.
 */
uint32_t map_has(void *key, hashmap *map, uint32_t key_size) { return set_has(key, map->set, key_size); }

hashset *map_copy(hashmap *map) {

    hashmap *copy = malloc(sizeof(hashmap));
    if (map == NULL) {
        printf("map_copy(): Failed to allocate %zu bytes for hashmap struct!\n", sizeof(hashmap));
        return NULL;
    }

    copy->set = set_copy(map->set);
    
    copy->value_size = map->value_size;
    copy->value_offsets = malloc(map->set->capacity * sizeof(*map->value_offsets));
    if (copy->value_offsets == NULL) {
        printf("map_copy(): Failed to allocate %zu bytes for value-offsets array!\n", map->set->capacity * sizeof(*map->value_offsets));
        set_free(copy->set);
        free(copy);
        return NULL;
    }
    memcpy(copy->value_offsets, map->value_offsets, map->set->capacity * sizeof(*map->value_offsets));

    copy->values = arena_copy(map->values);
    DEBUG_PRINTF("map_copy(): Copied map at %p\n", map);
    return copy;
}

void *map_at(uint32_t index, hashmap *map) { return set_at(index, map->set); }

void map_free(hashmap *map) {
    DEBUG_PRINTF("map_free(): Freeing set at address %p\n", map);
    set_free(map->set);
    free(map->value_offsets);
    arena_free(map->values);
    free(map);
}

/**
 * Resizes the set to the power of 2 which is at least double of its current size.
 * Rehashes all keys which are still present (not deleted).
 */
void set_resize(hashset *set) {
    
    uint32_t old_size = set->size;
    uint32_t new_capacity = 1 << (LOG2CEIL(old_size) + 1);
    DEBUG_PRINTF("set_resize(): Resizing hashset: %zu -> %zu\n", set->capacity, new_capacity);
    
    hashset *resized_set = new_set(new_capacity, set->elem_size, set->hash_func);
    hashset_iterator iter = get_hashset_iterator(set);
    
    if (set->elem_size >= 0) {
        for (void *item = set_iter_next(&iter); item; item = set_iter_next(&iter)) {
            set_add(item, resized_set, resized_set->elem_size);
        }
    } else {
        for (void *item = set_iter_next(&iter); item; item = set_iter_next(&iter)) {
            set_add(item, resized_set, resized_set->elem_size);
        }
    }

    free(set->offsets);
    free(set->occupied);
    arena_free(set->elems);
    *set = *resized_set;
    free(resized_set);
}

#endif  // MMM_HASHSET