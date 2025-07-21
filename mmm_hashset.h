#ifndef MMM_HASHSET
#define MMM_HASHSET

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "mmm_debug.h"
#include "mmm_arena.h"

typedef struct {
    arena *keys;  // keys live in this arena. Arena may be resized as necessary.
/*  array of offsets of the key's location in the `keys` arena. To be specific, the key can be found at `set->keys->ptr + set->offsets[hash]`.
    The value of 'hash' may be 0.
    However, 0 is used as a sentinel value for the offset itself, indicating an unused slot (SET_KEY_NOT_FOUND)
    (uint32_t) -1 is also a sentinel value, indicating a deleted slot (SET_KEY_DELETED)
*/
    uint32_t *offsets;

/*  key_size < 0: keys are dynamically sized, and the first `abs(key_size)` bytes indicate the size of the key.
                   For example, if key_size = -2, the first 2 bytes give the size of the key.
    key_size == 0: keys are null-terminated strings.
    key_size > 0: keys occupy a fixed number of bytes, given by `key_size`
*/
    int32_t key_size; 
    uint32_t size;  // no. of keys
    uint32_t load;  // no. of slots used in the hash table, including slots marked with DELETED.
    uint32_t capacity;  // always a power of 2
    uint32_t *occupied;  // dense array of encountered hashes. To be checked when iterating over the set.
    uint32_t occ_idx;  // the next available index of `occupied`. Equivalent to the 'used size' of the `occupied` array.
    uint32_t occ_packed;  // a boolean indicating if the `occupied` array has been packed by the iterator.
    uint32_t (*hash_func)(void *);  // the hash function
} hashset;

typedef struct {
    hashset *set;
    uint32_t dense_idx;  // the "left" pointer. The next explored key is relocated to this position.
    uint32_t explorer_idx;  // the "right" pointer. If left pointer reaches a free slot, right pointer explores until it finds an key.
    uint32_t num_iterations;
} hashset_iterator;

#define LOG2CEIL(X) (8 * sizeof(uint32_t) - __builtin_clzl(X) - 1 + (__builtin_popcount(X) != 1))
#define SET_KEY_DELETED ((uint32_t) -1)
#define SET_KEY_NOT_FOUND 0
#define SET_ERROR ((int64_t) (-1))
#define SET_GET_NEG_SIZE_KEY(KEY, NEG_SIZE, ACTUAL_SIZE) \
    ACTUAL_SIZE = 0; \
    do { for (int32_t i = 0; i < -NEG_SIZE; i++) ACTUAL_SIZE += (*(uint8_t *)(KEY++)) << (8 * i); } while (0)

#define FOR_KEY_IN_SET(K, S, code) do { \
    hashset_iterator iter = get_hashset_iterator(S); \
    for (void *K = set_iter_next(&iter); K; K = set_iter_next(&iter)) {code} \
} while (0)

void set_resize(hashset *set);

hashset *new_set(uint32_t capacity, int32_t key_size, uint32_t (*hash_func)(void *)) {

    uint32_t old_capacity = capacity;
    capacity = 1 << LOG2CEIL(capacity);
    DEBUG_PRINTF("new_set(): Creating hashset of capacity %d\n", capacity);
    
    hashset *set = malloc(sizeof(hashset));
    if (set == NULL) {
        printf("new_set(): Failed to allocate %zu bytes for hashset struct!\n", sizeof(hashset));
        return NULL;
    }

    set->offsets = calloc(capacity, sizeof(*set->offsets));
    if (set->offsets == NULL) {
        printf("new_set(): Failed to allocate %zu bytes for offsets array!\n", capacity * sizeof(*set->offsets));
        free(set);
        return NULL;
    }

    set->occupied = calloc(capacity, sizeof(*set->occupied));
    if (set->occupied == NULL) {
        printf("new_set(): Failed to allocate %zu bytes for occupied array!\n", capacity * sizeof(*set->occupied));
        free(set->offsets);
        free(set);
        return NULL;
    }
    set->occ_idx = 0;
    set->occ_packed = 1;

    uint32_t arena_size = capacity * (key_size <= 0 ? 256 : key_size);  // default variable arena size is 256 bytes per key
    set->keys = new_arena(arena_size);
    if (set->keys == NULL) {
        printf("new_set(): Failed to initialize keys arena of %zu bytes.\n", arena_size);
        free(set->occupied);
        free(set->offsets);
        free(set);
        return NULL;
    }

    a_malloc(1, set->keys);  // if set->offsets[hash] == 0, that indicates no key at that hash value.
    set->size = set->load = 0;
    set->capacity = capacity;
    set->key_size = key_size;
    set->hash_func = hash_func;
    return set;
}

uint8_t set_compare_to_key(void *key, void *item, int32_t key_size, int32_t item_size) {

    if (item_size == 0) return strcmp(key, item) == 0;
    if (key_size > 0) return key_size == item_size && memcmp(key, item, item_size) == 0;
    uint32_t actual_size = 0;
    SET_GET_NEG_SIZE_KEY(key, key_size, actual_size);
    return actual_size == item_size && memcmp(key, item, actual_size) == 0;
}

/**
 * Searches for `key` in `set`.
 * Returns the final hash position of the key if it exists, else SET_ERROR.
 */
int64_t set_get_hash(void *key, hashset *set, uint32_t key_size) {

    uint32_t hash = (set->hash_func(key) * 11400714819323198485llu) & (set->capacity - 1);  // Fibonacci hash
    while (set->offsets[hash] != SET_KEY_NOT_FOUND) {
        if (set->offsets[hash] != SET_KEY_DELETED && set_compare_to_key(key, set->keys->ptr + set->offsets[hash], key_size, set->key_size)) {
            return hash;
        }
        hash = ((hash * 5) + 1) & (set->capacity - 1);
    }
    return SET_ERROR;
}

/**
 * Adds `key` to `set`, checking first if it already exists.
 * Returns the final hash position of the key if successfully added or key already exists, else SET_ERROR.
 */
int64_t set_add(void *key, hashset *set, uint32_t key_size) {

    if (set->key_size >= 0 && set->key_size != key_size) {
        puts("set_add(): Key is not the correct size!");
        return SET_ERROR;
    }

    if (set->load * 3 > set->capacity * 2) set_resize(set);

    int64_t add_to_hash = -1;
    uint32_t hash = (set->hash_func(key) * 11400714819323198485llu) & (set->capacity - 1);  // Fibonacci hash
    while (1) {
        if (set->offsets[hash] == SET_KEY_NOT_FOUND) {
            if (add_to_hash != -1) hash = add_to_hash;  // reset hash to the position of the first deleted key, if any.
            break;
        }
        if (set->offsets[hash] == SET_KEY_DELETED) {
            if (add_to_hash == -1) add_to_hash = hash;  // remember the first deleted key, if any.
        } else if (set_compare_to_key(key, set->keys->ptr + set->offsets[hash], key_size, set->key_size)) {
            DEBUG_PRINTF("set_add(): Key already exists in set!");
            return hash;
        }
        hash = ((hash * 5) + 1) & (set->capacity - 1);
    }

    if (set->offsets[hash] != SET_KEY_DELETED) {  // Adding key to fresh slot; load increases.
        set->load++;
        if (set->occ_packed) {  // if the `occupied` array is packed, we place the new hash at the right spot.
            set->occupied[set->occ_idx++] = set->occupied[set->size];
            set->occupied[set->size] = hash;  // We only add the hash to the dense array if it has not been added before.
        } else {                              // Hashes are never deleted from the dense array, only moved to the end.
            set->occupied[set->occ_idx++] = hash;  // if the `occupied` array is not packed, add the new hash to the end.
        }
    }

    set->size++;
    if (set->key_size < 0) {  // variable key size
        set->offsets[hash] = a_malloc(key_size - set->key_size, set->keys) - set->keys->ptr;
        if (set->offsets[hash] == 0) {
            puts("set_add(): Failed to allocate memory in arena!");
            return SET_ERROR;
        }
        uint32_t key_size_copy = key_size;
        for (int32_t i = 0; i < -set->key_size; i++) {
            *(uint8_t *)(set->keys->ptr + set->offsets[hash] + i) = key_size_copy & 0xFF;
            key_size_copy >>= 8;
        }
        memcpy(set->keys->ptr + set->offsets[hash] - set->key_size, key, key_size);
    } else {
        if (key_size == 0) key_size = strlen(key) + 1;  // null-terminated strings
        set->offsets[hash] = a_malloc(key_size, set->keys) - set->keys->ptr;
        if (set->offsets[hash] == 0) {
            puts("set_add(): Failed to allocate memory in arena!");
            return SET_ERROR;
        }
        memcpy(set->keys->ptr + set->offsets[hash], key, key_size);
    }
    DEBUG_PRINTF("set_add(): Adding key to hash index %d\n", hash);
    return hash;
}

/**
 * Removes `key` from `set` if it exists.
 * Returns the final hash position of the key if successfully deleted, else SET_ERROR.
 */
int64_t set_delete(void *key, hashset *set, uint32_t key_size) {

    int64_t hash = set_get_hash(key, set, key_size);
    if (hash != SET_ERROR) {
        DEBUG_PRINTF("set_delete(): Deleting key at hash %d\n", hash);
        set->offsets[hash] = SET_KEY_DELETED;
        set->occ_packed = 0;
        set->size--;
        return hash;
    }
    DEBUG_PRINTF("set_delete(): Key not found!");
    return SET_ERROR;
}


/**
 * A 'boolean' wrapper around `set_get_hash`.
 * Returns 1 if the key exists, else 0.
 */
uint32_t set_has(void *key, hashset *set, uint32_t key_size) { return set_get_hash(key, set, key_size) != SET_ERROR; }

hashset *set_copy(hashset *set) {
    hashset *copy = new_set(set->capacity, set->key_size, set->hash_func);
    arena_free(copy->keys);
    copy->keys = arena_copy(set->keys);
    memcpy(copy->offsets, set->offsets, copy->capacity * sizeof(*copy->offsets));
    memcpy(copy->occupied, set->occupied, copy->capacity * sizeof(*copy->occupied));
    copy->occ_idx = set->occ_idx;
    copy->occ_packed = set->occ_packed;
    copy->size = set->size;
    copy->load = set->load;
    DEBUG_PRINTF("set_copy(): Copied set at %p\n", set);
    return copy;
}

hashset_iterator get_hashset_iterator(hashset *set) {
    hashset_iterator it;
    it.dense_idx = it.explorer_idx = 0;
    it.set = set;
    it.num_iterations = set->size;
    return it;
}

// returns address of next key, and also packs the `occupied` array. returns NULL if iteration has ended.
void *set_iter_next(hashset_iterator *it) {
    hashset *set = it->set;
    if (it->dense_idx >= it->num_iterations) { DEBUG_PRINTF("set_iter_next(): End of iteration\n"); set->occ_packed = (it->dense_idx == set->size); return NULL; }
    if (it->explorer_idx >= set->occ_idx) { DEBUG_PRINTF("set_iter_next(): Iteration incomplete (1); unexpected end of array reached!\n"); return NULL; }
    if (set->offsets[set->occupied[it->dense_idx]] != SET_KEY_DELETED) return set->keys->ptr + set->offsets[set->occupied[it->dense_idx++]];
    
    // Now we explore from it->explorer_idx onwards to locate next key.
    if (it->explorer_idx <= it->dense_idx) it->explorer_idx = it->dense_idx + 1;
    while (1) {
        if (it->explorer_idx >= set->occ_idx) { DEBUG_PRINTF("set_iter_next(): Iteration incomplete (2); unexpected end of array reached!\n"); return NULL; }
        if (set->offsets[set->occupied[it->explorer_idx]] != SET_KEY_DELETED) {
            DEBUG_PRINTF("set_iter_next(): Swapping set->occupied [%d] and [%d]\n", it->explorer_idx, it->dense_idx);
            uint32_t temp = set->occupied[it->dense_idx];
            set->occupied[it->dense_idx] = set->occupied[it->explorer_idx];
            set->occupied[it->explorer_idx] = temp;
            return set->keys->ptr + set->offsets[set->occupied[it->dense_idx++]];
        }
        it->explorer_idx++;
    }
}

void *set_at(uint32_t index, hashset *set) {

    if (index >= set->size) { printf("set_at(): Array index [%d] out of bounds (set->size == %d)", index, set->size); return NULL; }
    if (!set->occ_packed) FOR_KEY_IN_SET(key, set, 1;);
    return set->keys->ptr + set->offsets[set->occupied[index]];
}

void set_free(hashset *set) {
    DEBUG_PRINTF("set_free(): Freeing set at address %p\n", set);
    free(set->offsets);
    free(set->occupied);
    arena_free(set->keys);
    free(set);
}

/**
 * Resizes the set to the power of 2 which is at least double of its current size.
 * Rehashes all keys which are still present (not deleted).
 */
void set_resize(hashset *set) {
    
    uint32_t old_size = set->size;
    uint32_t new_capacity = 1 << (LOG2CEIL(old_size) + 1);
    DEBUG_PRINTF("set_resize(): Resizing hashset: %zu -> %zu\n", set->capacity, new_capacity);
    
    hashset *resized_set = new_set(new_capacity, set->key_size, set->hash_func);
    
    if (set->key_size >= 0) {
        FOR_KEY_IN_SET(key, set,
            set_add(key, resized_set, resized_set->key_size);
        );
    } else {
        FOR_KEY_IN_SET(key, set,
            uint32_t actual_size = 0;
            SET_GET_NEG_SIZE_KEY(key, resized_set->key_size, actual_size);
            set_add(key, resized_set, actual_size);
        );
    }

    free(set->offsets);
    free(set->occupied);
    arena_free(set->keys);
    *set = *resized_set;
    free(resized_set);
}

#endif  // MMM_HASHSET