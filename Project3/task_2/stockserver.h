#ifndef __STOCK_SERVER_H__
#define __STOCK_SERVER_H__

#include "csapp.h"
#include "sbuf.h"

#define NTHREADS 256
#define SBUFSIZE 1024

/* Represents stock item */
typedef struct {
    int     ID;
    int     stock_left;
    int     price;
    int     readcnt;
    sem_t   mutex;
    sem_t   w;
} item;

/* Node of binary tree */
typedef struct _Node {
    item            data;
    struct _Node    *left;
    struct _Node    *right;
} Node;

void    *thread(void *vargp);
void    service_client(char *buf, int connfd);

Node    *init_stock_data();
void    update_stock_file();
Node    *insert_node(Node *root, int stock_ID, int stock_left, int stock_price);
void    buy_stock(Node *node, int connfd, int stock_ID, int num_dealing);
void    sell_stock(Node *node, int connfd, int stock_ID, int num_dealing);
void    free_stock_tree(Node *root);
void    read_stock_data(Node *root, char *buf);


#endif