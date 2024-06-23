#include "main.h"

bool list_less(const struct list_elem *a, const struct list_elem *b, void *aux) {
    struct list_item *a_item = list_entry(a, struct list_item, elem);
    struct list_item *b_item = list_entry(b, struct list_item, elem);
    if (a_item->data < b_item->data)
        return true;
    else
        return false;
}

bool hash_less(const struct hash_elem *a, const struct hash_elem *b, void *aux) {
    if (a->data < b->data)
        return true;
    else
        return false;
}

void hash_action_print(struct hash_elem *e, void *aux) {
    printf("%d ", e->data);
}

void hash_action_square(struct hash_elem *e, void *aux) {
    int data = e->data;

    e->data = data * data;
}

void hash_action_triple(struct hash_elem *e, void *aux) {
    int data = e->data;

    e->data = data * data * data;
}

unsigned hash_hash_int(const struct hash_elem *e, void *aux) {
    return hash_int(e->data);
}

void hash_destructor(struct hash_elem *e, void *aux) {
    free(e);
}

void list_print(struct list *list);
void list_destroy(struct list *list);
void list_func(struct list *list_arr, char **toks);
void hash_func(struct hash *hash_arr, char **toks);
void bitmap_print(struct bitmap *bitmap);
void bitmap_func(struct bitmap **bitmap_arr, char **toks);


int main(void) {
    struct list list_arr[10];
    struct hash hash_arr[10];
    struct bitmap *bitmap_arr[10];

    char buf[128];
    char *toks[6];
    while (strcmp(buf, "quit")) {
        fgets(buf, sizeof(buf), stdin);
        buf[strlen(buf) - 1] = '\0';
        char *tok = strtok(buf, " ");
        int i;
        for (i = 0; tok; i++) {
            toks[i] = tok;
            tok = strtok(NULL, " ");
        }
        toks[i] = NULL;

        if (!strcmp(toks[0], "create")) {
            if (!strcmp(toks[1], "list"))
                list_init(&list_arr[toks[2][4] - '0']);
            if (!strcmp(toks[1], "hashtable")) {
                bool tmp = hash_init(&hash_arr[toks[2][4] - '0'], hash_hash_int, hash_less, NULL);
                assert(tmp);
            }
            if (!strcmp(toks[1], "bitmap")) {
                struct bitmap *bitmap = bitmap_create(atoi(toks[3]));
                bitmap_arr[toks[2][2] - '0'] = bitmap;
                assert(bitmap != NULL);
            }
        }

        if (!strcmp(toks[0], "dumpdata")) {
            if (strstr(toks[1], "list"))
                list_print(&list_arr[toks[1][4] - '0']);
            if (strstr(toks[1], "hash")) {
                struct hash *hash  = &hash_arr[toks[1][4] - '0'];
                if (hash_empty(hash))
                    continue;
                hash_apply(hash, hash_action_print);
                printf("\n");
            }
            if (strstr(toks[1], "bm")) {
                struct bitmap *bitmap = bitmap_arr[toks[1][2] - '0'];
                assert(bitmap != NULL);
                bitmap_print(bitmap);
            }
        }

        if (!strcmp(toks[0], "delete")) {
            if (strstr(toks[1], "list"))
                list_destroy(&list_arr[toks[1][4] - '0']);
            if (strstr(toks[1], "hash"))
                hash_destroy(&hash_arr[toks[1][4] - '0'], hash_destructor);
            if (strstr(toks[1], "bm"))
                bitmap_destroy(bitmap_arr[toks[1][2] - '0']);

        }

        if (strstr(toks[0], "list"))
            list_func(list_arr, toks);

        if (strstr(toks[0], "hash"))
            hash_func(hash_arr, toks);

        if (strstr(toks[0], "bitmap"))
            bitmap_func(bitmap_arr, toks);
    }

    return 0;
}

void list_print(struct list *list) {
    struct list_elem *iter = list_begin(list);

    if (list_empty(list))
        return;

    for (iter; iter != list_end(list); iter = iter->next) {
        printf("%d ", list_entry(iter, struct list_item, elem)->data);
    }

    printf("\n");
}

void list_destroy(struct list *list) {
    struct list_item *item;

    if (!list)
        return;
    
    while (!list_empty(list)) {
        item = list_entry(list_pop_front(list), struct list_item, elem);
        free(item);
    }
}

void list_func(struct list *list_arr, char **toks) {
    if (!strcmp(toks[0], "list_front")) {
        struct list *list = &list_arr[toks[1][4] - '0'];

        printf("%d\n", list_entry(list_front(list), struct list_item, elem)->data);
    }

    if (!strcmp(toks[0], "list_back")) {
        struct list *list = &list_arr[toks[1][4] - '0'];

        printf("%d\n", list_entry(list_back(list), struct list_item, elem)->data);
    }

    if (!strcmp(toks[0], "list_insert")) {
        struct list *list = &list_arr[toks[1][4] - '0'];
        int pos = atoi(toks[2]);
        struct list_item *item = (struct list_item *)malloc(sizeof(struct list_item));
        struct list_elem *elem = &item->elem;
        struct list_elem *before = list_begin(list);

        item->data = atoi(toks[3]);

        for (int i = 0; i < pos; i++)
            before = before->next;

        list_insert(before, elem);
    }

    if (!strcmp(toks[0], "list_insert_ordered")) {
        struct list *list = &list_arr[toks[1][4] - '0'];
        struct list_item *item = (struct list_item *)malloc(sizeof(struct list_item));
        struct list_elem *elem = &item->elem;

        item->data = atoi(toks[2]);
        list_insert_ordered(list, elem, list_less, NULL);
    }

    if (!strcmp(toks[0], "list_push_front")) {
        struct list *list = &list_arr[toks[1][4] - '0'];
        struct list_item *item = (struct list_item *)malloc(sizeof(struct list_item));
        struct list_elem *elem = &item->elem;

        item->data = atoi(toks[2]);
        list_push_front(list, elem);
    }

    if (!strcmp(toks[0], "list_push_back")) {
        struct list *list = &list_arr[toks[1][4] - '0'];
        struct list_item *item = (struct list_item *)malloc(sizeof(struct list_item));
        struct list_elem *elem = &item->elem;

        item->data = atoi(toks[2]);
        list_push_back(list, elem);
    }

    if (!strcmp(toks[0], "list_pop_front")) {
        struct list *list = &list_arr[toks[1][4] - '0'];

        list_pop_front(list);
    }

    if (!strcmp(toks[0], "list_pop_back")) {
        struct list *list = &list_arr[toks[1][4] - '0'];

        list_pop_back(list);
    }

    if (!strcmp(toks[0], "list_empty")) {
        struct list *list = &list_arr[toks[1][4] - '0'];

        if (list_empty(list))
            printf("true\n");
        else
            printf("false\n");
    }

    if (!strcmp(toks[0], "list_size")) {
        struct list *list = &list_arr[toks[1][4] - '0'];

        printf("%zu\n", list_size(list));
    }

    if (!strcmp(toks[0], "list_max")) {
        struct list *list = &list_arr[toks[1][4] - '0'];

        printf("%d\n", list_entry(list_max(list, list_less, NULL), struct list_item, elem)->data);
    }

    if (!strcmp(toks[0], "list_min")) {
        struct list *list = &list_arr[toks[1][4] - '0'];

        printf("%d\n", list_entry(list_min(list, list_less, NULL), struct list_item, elem)->data);
    }

    if (!strcmp(toks[0], "list_remove")) {
        struct list *list = &list_arr[toks[1][4] - '0'];
        int pos = atoi(toks[2]);
        struct list_elem *elem = list_begin(list);
        struct list_item *item;

        for (int i = 0; i < pos; i++)
            elem = elem->next;

        item = list_entry(elem, struct list_item, elem);
        elem = list_remove(elem);
        free(item);
    }

    if (!strcmp(toks[0], "list_reverse")) {
        struct list *list = &list_arr[toks[1][4] - '0'];

        list_reverse(list);
    }

    if (!strcmp(toks[0], "list_shuffle")) {
        struct list *list = &list_arr[toks[1][4] - '0'];

        list_shuffle(list);
    }

    if (!strcmp(toks[0], "list_sort")) {
        struct list *list = &list_arr[toks[1][4] - '0'];

        list_sort(list, list_less, NULL);
    }

    if (!strcmp(toks[0], "list_splice")) {
        struct list *list_a = &list_arr[toks[1][4] - '0'];
        struct list *list_b = &list_arr[toks[3][4] - '0'];
        struct list_elem *before = list_begin(list_a);
        struct list_elem *first = list_begin(list_b);
        struct list_elem *last = list_begin(list_b);
        int pos_before = atoi(toks[2]);
        int pos_first = atoi(toks[4]);
        int pos_last = atoi(toks[5]);

        for (int i = 0; i < pos_before; i++)
            before = before->next;

        for (int i = 0; i < pos_first; i++)
            first = first->next;

        for (int i = 0; i < pos_last; i++)
            last = last->next;
        
        list_splice(before, first, last);
    }

    if (!strcmp(toks[0], "list_swap")) {
        struct list *list = &list_arr[toks[1][4] - '0'];
        struct list_elem *elem_a = list_begin(list);
        struct list_elem *elem_b = list_begin(list);
        int pos_a = atoi(toks[2]);
        int pos_b = atoi(toks[3]);

        for (int i = 0; i < pos_a; i++)
            elem_a = elem_a->next;

        for (int i = 0; i < pos_b; i++)
            elem_b = elem_b->next;
        
        list_swap(elem_a, elem_b);
    }

    if (!strcmp(toks[0], "list_unique")) {
        if (toks[2] == NULL) {
            struct list *list = &list_arr[toks[1][4] - '0'];

            list_unique(list, NULL, list_less, NULL);
        } else {
            struct list *list = &list_arr[toks[1][4] - '0'];
            struct list *dup = &list_arr[toks[2][4] - '0'];

            list_unique(list, dup, list_less, NULL);
        }
    }
}

void hash_func(struct hash *hash_arr, char **toks) {
    if (!strcmp(toks[0], "hash_insert")) {
        struct hash *hash = &hash_arr[toks[1][4] - '0'];
        struct hash_elem *hash_elem = (struct hash_elem *)malloc(sizeof(struct hash_elem));
        struct hash_elem *temp;

        hash_elem->data = atoi(toks[2]);
        temp = hash_insert(hash, hash_elem);
    }

    if (!strcmp(toks[0], "hash_find")) {
        struct hash *hash = &hash_arr[toks[1][4] - '0'];
        struct hash_elem *hash_elem;
        struct hash_elem to_find;

        to_find.data = atoi(toks[2]);
        hash_elem = hash_find(hash, &to_find);
        if (hash_elem)
            printf("%d\n", hash_elem->data);
    }

    if (!strcmp(toks[0], "hash_delete")) {
        struct hash *hash = &hash_arr[toks[1][4] - '0'];
        struct hash_elem *hash_elem;
        struct hash_elem to_find;

        to_find.data = atoi(toks[2]);
        hash_elem = hash_delete(hash, &to_find);
        if (hash_elem)
            hash_destructor(hash_elem, NULL);
    }

    if (!strcmp(toks[0], "hash_apply")) {
        struct hash *hash = &hash_arr[toks[1][4] - '0'];

        if (!strcmp(toks[2], "square"))
            hash_apply(hash, hash_action_square);
        
        if (!strcmp(toks[2], "triple"))
            hash_apply(hash, hash_action_triple);
    }

    if (!strcmp(toks[0], "hash_replace")) {
        struct hash *hash = &hash_arr[toks[1][4] - '0'];
        struct hash_elem *old;
        struct hash_elem *new = (struct hash_elem *)malloc(sizeof(struct hash_elem));

        new->data = atoi(toks[2]);
        old = hash_replace(hash, new);
        if (old)
            hash_destructor(old, NULL);
    }

    if (!strcmp(toks[0], "hash_empty")) {
        struct hash *hash = &hash_arr[toks[1][4] - '0'];

        if (hash_empty(hash))
            printf("true\n");
        else
            printf("false\n");
    }

    if (!strcmp(toks[0], "hash_size")) {
        struct hash *hash = &hash_arr[toks[1][4] - '0'];

        printf("%zu\n", hash_size(hash));
    }

    if (!strcmp(toks[0], "hash_clear")) {
        struct hash *hash = &hash_arr[toks[1][4] - '0'];

        hash_clear(hash, hash_destructor);
    }
}

void bitmap_print(struct bitmap *bitmap) {
    elem_type b = 1;

    for (int i = 0; i < bitmap->bit_cnt; i++) {
        if (*(bitmap->bits) & b)
            printf("1");
        else
            printf("0");
        b = b << 1;
    }
    printf("\n");
}

void bitmap_func(struct bitmap **bitmap_arr, char **toks) {
    if (!strcmp(toks[0], "bitmap_mark")) {
        struct bitmap *bitmap = bitmap_arr[toks[1][2] - '0'];
        size_t bit_idx = (size_t)atoi(toks[2]);

        bitmap_mark(bitmap, bit_idx);
    }

    if (!strcmp(toks[0], "bitmap_set")) {
        struct bitmap *bitmap = bitmap_arr[toks[1][2] - '0'];
        size_t bit_idx = (size_t)atoi(toks[2]);
        
        if (!strcmp(toks[3], "true"))
            bitmap_set(bitmap, bit_idx, true);
        else if (!strcmp(toks[3], "false"))
            bitmap_set(bitmap, bit_idx, false);
    }

    if (!strcmp(toks[0], "bitmap_reset")) {
        struct bitmap *bitmap = bitmap_arr[toks[1][2] - '0'];
        size_t bit_idx = (size_t)atoi(toks[2]); 

        bitmap_reset(bitmap, bit_idx);  
    }

    if (!strcmp(toks[0], "bitmap_flip")) {
        struct bitmap *bitmap = bitmap_arr[toks[1][2] - '0'];
        size_t bit_idx = (size_t)atoi(toks[2]); 

        bitmap_flip(bitmap, bit_idx);
    }

    if (!strcmp(toks[0], "bitmap_test")) {
        struct bitmap *bitmap = bitmap_arr[toks[1][2] - '0'];
        size_t bit_idx = (size_t)atoi(toks[2]);
        bool value = bitmap_test(bitmap, bit_idx);

        if (value)
            printf("true\n");
        else
            printf("false\n");
    }

    if (!strcmp(toks[0], "bitmap_set_all")) {
        struct bitmap *bitmap = bitmap_arr[toks[1][2] - '0'];

        if (!strcmp(toks[2], "true"))
            bitmap_set_all(bitmap, true);
        else if (!strcmp(toks[2], "false"))
            bitmap_set_all(bitmap, false);
    }

    if (!strcmp(toks[0], "bitmap_set_multiple")) {
        struct bitmap *bitmap = bitmap_arr[toks[1][2] - '0'];
        size_t start = (size_t)atoi(toks[2]);
        size_t cnt = (size_t)atoi(toks[3]);

        if (!strcmp(toks[4], "true"))
            bitmap_set_multiple(bitmap, start, cnt, true);
        else if (!strcmp(toks[4], "false"))
            bitmap_set_multiple(bitmap, start, cnt, false);
    }

    if (!strcmp(toks[0], "bitmap_count")) {
        struct bitmap *bitmap = bitmap_arr[toks[1][2] - '0'];
        size_t start = (size_t)atoi(toks[2]);
        size_t cnt = (size_t)atoi(toks[3]);
        size_t value;

        if (!strcmp(toks[4], "true"))
            value = bitmap_count(bitmap, start, cnt, true);
        else if (!strcmp(toks[4], "false"))
            value = bitmap_count(bitmap, start, cnt, false);
        
        printf("%zu\n", value);
    }

    if (!strcmp(toks[0], "bitmap_contains")) {
        struct bitmap *bitmap = bitmap_arr[toks[1][2] - '0'];
        size_t start = (size_t)atoi(toks[2]);
        size_t cnt = (size_t)atoi(toks[3]);
        bool value;

        if (!strcmp(toks[4], "true"))
            value = bitmap_contains(bitmap, start, cnt, true);
        else if (!strcmp(toks[4], "false"))
            value = bitmap_contains(bitmap, start, cnt, false);
        
        if (value)
            printf("true\n");
        else
            printf("false\n");
    }

    if (!strcmp(toks[0], "bitmap_any")) {
        struct bitmap *bitmap = bitmap_arr[toks[1][2] - '0'];
        size_t start = (size_t)atoi(toks[2]);
        size_t cnt = (size_t)atoi(toks[3]);
        bool value = bitmap_any(bitmap, start, cnt);

        if (value)
            printf("true\n");
        else
            printf("false\n");
    }

    if (!strcmp(toks[0], "bitmap_none")) {
        struct bitmap *bitmap = bitmap_arr[toks[1][2] - '0'];
        size_t start = (size_t)atoi(toks[2]);
        size_t cnt = (size_t)atoi(toks[3]);
        bool value = bitmap_none(bitmap, start, cnt);

        if (value)
            printf("true\n");
        else
            printf("false\n");
    }

    if (!strcmp(toks[0], "bitmap_all")) {
        struct bitmap *bitmap = bitmap_arr[toks[1][2] - '0'];
        size_t start = (size_t)atoi(toks[2]);
        size_t cnt = (size_t)atoi(toks[3]);
        bool value = bitmap_all(bitmap, start, cnt);

        if (value)
            printf("true\n");
        else
            printf("false\n");
    }

    if (!strcmp(toks[0], "bitmap_scan")) {
        struct bitmap *bitmap = bitmap_arr[toks[1][2] - '0'];
        size_t start = (size_t)atoi(toks[2]);
        size_t cnt = (size_t)atoi(toks[3]);
        size_t value;

        if (!strcmp(toks[4], "true"))
            value = bitmap_scan(bitmap, start, cnt, true);
        else if (!strcmp(toks[4], "false"))
            value = bitmap_scan(bitmap, start, cnt, false);
        
        printf("%zu\n", value);
    }

    if (!strcmp(toks[0], "bitmap_scan_and_flip")) {
        struct bitmap *bitmap = bitmap_arr[toks[1][2] - '0'];
        size_t start = (size_t)atoi(toks[2]);
        size_t cnt = (size_t)atoi(toks[3]);
        size_t value;

        if (!strcmp(toks[4], "true"))
            value = bitmap_scan_and_flip(bitmap, start, cnt, true);
        else if (!strcmp(toks[4], "false"))
            value = bitmap_scan_and_flip(bitmap, start, cnt, false);
        
        printf("%zu\n", value);
    }

    if (!strcmp(toks[0], "bitmap_size")) {
        struct bitmap *bitmap = bitmap_arr[toks[1][2] - '0'];

        printf("%zu\n", bitmap_size(bitmap));
    }

    if (!strcmp(toks[0], "bitmap_expand")) {
        struct bitmap *bitmap = bitmap_arr[toks[1][2] - '0'];
        int size = atoi(toks[2]);

        bitmap = bitmap_expand(bitmap, size);
        assert(bitmap != NULL);
    }

    if (!strcmp(toks[0], "bitmap_dump")) {
        struct bitmap *bitmap = bitmap_arr[toks[1][2] - '0'];

        bitmap_dump(bitmap);
    }
}