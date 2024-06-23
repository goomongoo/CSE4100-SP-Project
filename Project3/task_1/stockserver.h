#ifndef __STOCK_SERVER_H__
#define __STOCK_SERVER_H__

#include "csapp.h"

/* Pool of connected descriptors */
typedef struct {
    int     maxfd;                  /* Largest descriptor */
    int     nready;                 /* Number of ready descriptors from select */
    int     nclient;
    int     maxi;                   /* Highwater index into client array */
    int     clientfd[FD_SETSIZE];   /* Set of active descriptors */
    fd_set  read_set;               /* Set of all active destriptors */
    fd_set  ready_set;              /* Subset of descriptors ready for reading */
    rio_t   clientrio[FD_SETSIZE];  /* Set of active read buffers */
} pool;

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

void    init_pool(int listenfd, pool *p);
void    add_client(int connfd, pool *p);
void    check_clients(pool *p);
void    service_client(char *buf, int connfd);

Node    *init_stock_data();
void    update_stock_file();
Node    *insert_node(Node *root, int stock_ID, int stock_left, int stock_price);
void    buy_stock(Node *node, int connfd, int stock_ID, int num_dealing);
void    sell_stock(Node *node, int connfd, int stock_ID, int num_dealing);
void    free_stock_tree(Node *root);
void    read_stock_data(Node *root, char *buf);


#endif