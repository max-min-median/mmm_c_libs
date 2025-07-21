#define DEBUG
#include "mmm_hashset.h"

uint32_t hash(void *item) {
    // printf("hash(): %d\n", *((int *) item));
    return *(uint32_t *) item;
}

void test1() {

    typedef struct {
        int a;
        int b;
        double c;
    } test_obj;
    printf("Size of test_obj: %d\n", sizeof(test_obj));
    test_obj o1 = {.a = 1111, .b = 910, .c = 3.14159};
    test_obj o2 = {.a = -2222, .b = 235, .c = 593.141};
    test_obj o3 = {.a = 3333, .b = 419, .c = 314.159};
    test_obj o4 = {.a = -4444, .b = 906, .c = 3141.59};
    test_obj o5 = {.a = 5555, .b = 1432, .c = 931.415};
    test_obj o6 = {.a = -6666, .b = -4345, .c = 1415.93};
    test_obj o7 = {.a = 77777777, .b = -4345, .c = 124515.93};
    test_obj o8 = {.a = -888888, .b = -4345, .c = 332.178};
    test_obj o9 = {.a = 9999, .b = 12464327, .c = 2.71828};
    test_obj o10 = {.a = -101010, .b = -33191, .c = 12.34567};
    hashset *h = new_set(100, -1, hash);
    set_add(&o1, h, 16);  // 35
    set_add(&o2, h, 16);  // 58
    set_add(&o3, h, 16);  // 105
    set_add(&o4, h, 16);  // 116
    set_delete(&o4, h, 16); 
    // set_add(&o4, h, 16);
    set_add(&o5, h, 16);  // 47
    set_delete(&o5, h, 16);  // 47    
    set_add(&o6, h, 16);  // 46
    set_add(&o7, h, 16);  // 69
    set_add(&o8, h, 16);  // 104

    hashset *copy = set_copy(h);

    set_add(&o9, h, 16);
    set_add(&o10, copy, 16);

    putchar('{'); 
    FOR_KEY_IN_SET(item, h,
        test_obj o = *(test_obj *) ++item;
        printf("%d %d %f | ", o.a, o.b, o.c);
    );
    puts("}");

    putchar('{'); 
    FOR_KEY_IN_SET(item, copy,
        test_obj o = *(test_obj *) ++item;
        printf("%d %d %f | ", o.a, o.b, o.c);
    );
    puts("}");

    set_add(&o5, h, 16);  // 47
    set_add(&o5, copy, 16);  // 47

    putchar('{'); 
    FOR_KEY_IN_SET(item, h,
        test_obj o = *(test_obj *) ++item;
        printf("%d %d %f | ", o.a, o.b, o.c);
    );
    puts("}");

    putchar('{'); 
    FOR_KEY_IN_SET(item, copy,
        test_obj o = *(test_obj *) ++item;
        printf("%d %d %f | ", o.a, o.b, o.c);
    );
    puts("}");

}

void test2() {
    hashset *h = new_set(32, sizeof(int), hash);
    int o0 = 0;
    int o1 = 1;
    int o2 = 2;
    int o3 = 3;
    int o4 = 4;
    int o5 = 5;
    int o6 = 6;
    int o7 = 7;
    int o8 = 8;
    set_add(&o1, h, 4);
    set_add(&o2, h, 4);
    set_add(&o3, h, 4);
    set_add(&o4, h, 4);

    set_delete(&o3, h, 4);

    set_add(&o5, h, 4);
    set_add(&o6, h, 4);
    set_add(&o7, h, 4);
    set_add(&o8, h, 4);

    set_delete(&o6, h, 4);
    set_delete(&o6, h, 4);

    hashset_iterator iter = get_hashset_iterator(h);
    putchar('{');
    for (void *item = set_iter_next(&iter); item; item = set_iter_next(&iter)) {
        printf("%d ", *(int *)(item));
    }
    puts("}");

    iter = get_hashset_iterator(h);
    putchar('{');
    for (void *item = set_iter_next(&iter); item; item = set_iter_next(&iter)) {
        printf("%d ", *(int *)(item));
    }
    puts("}");
}


void test3() {

    hashset *h = new_set(100, 0, hash);
    set_add("This", h, 0);
    set_add("is", h, 0);
    set_add("a", h, 0);
    set_add("somewhat", h, 0);
    set_add("comprehensive", h, 0);
    set_add("comprehensive", h, 0);
    set_add("test", h, 0);
    hashset *h2 = set_copy(h);
    int64_t hsh = set_delete("a", h2, 0);
    printf("Deleting 'a' at %d\n", hsh);
    set_add("of whether this", h, 0);
    set_add("hashset() implementation written in C", h, 0);
    set_add("works", h, 0);
    set_delete("hashset() implementation written in C", h, 0);
    set_delete("works", h, 0);
    set_add("hashset() implementation written in C", h, 0);
    set_add("works", h, 0);
    set_resize(h);

    putchar('{');
    FOR_KEY_IN_SET(item, h,
        printf("'%s' | ", item);
    );
    puts("}");

    putchar('{');
    FOR_KEY_IN_SET(item, h2,
        printf("'%s' | ", item);
    );
    puts("}");

    printf("h has 'somewhat': %s\n", set_has("somewhat", h, 0) ? "YES" : "NO");
    printf("h has 'comprehend': %s\n", set_has("comprehend", h, 0) ? "YES" : "NO");
    printf("h has 'This': %s\n", set_has("This", h, 0) ? "YES" : "NO");
    printf("h2 has 'a': %s\n", set_has("a", h2, 0) ? "YES" : "NO");
    printf("h2 has 'comprehensive': %s\n", set_has("comprehensive", h2, 0) ? "YES" : "NO");

    printf("h set_at(1): %s\n", (char *) set_at(1, h));
    printf("h set_at(2): %s\n", (char *) set_at(2, h));
    printf("h set_at(3): %s\n", (char *) set_at(3, h));

}

int main(void) {
    test1();
    test2();
    test3();
}