#include "csapp.h"
#include "stockserver.h"

Node    *stock_tree = NULL;
sbuf_t  sbuf;

static int      byte_cnt;
static sem_t    mutex;

/* SIGINT handler - begin */
void sigint_handler(int sig)
{
    P(&mutex);
    sbuf_deinit(&sbuf);
    update_stock_file();
    free_stock_tree(stock_tree);
    V(&mutex);
    exit(0);
}
/* SIGINT handler - end */

/* main - begin */
int main(int argc, char **argv) 
{
    int                     listenfd;
    int                     connfd;
    char                    client_hostname[MAXLINE];
    char                    client_port[MAXLINE];
    socklen_t               clientlen;
    struct sockaddr_storage clientaddr;
    pthread_t               tid;

    if (argc != 2)
    {
	    fprintf(stderr, "usage: %s <port>\n", argv[0]);
	    exit(0);
    }

    Signal(SIGINT, sigint_handler);
    stock_tree = init_stock_data();
    listenfd = Open_listenfd(argv[1]);

    sbuf_init(&sbuf, SBUFSIZE);
    for (int i = 0; i < NTHREADS; i++)
        Pthread_create(&tid, NULL, thread, NULL);
    
    while (1)
    {
        clientlen = sizeof(struct sockaddr_storage);
        connfd = Accept(listenfd, (SA *) &clientaddr, &clientlen);
        sbuf_insert(&sbuf, connfd);
        Getnameinfo((SA *) &clientaddr, clientlen, client_hostname, MAXLINE, client_port, MAXLINE, 0);
        printf("Connected to (%s, %s)\n", client_hostname, client_port);
    }

    return 0;
}
/* main - begin */

/* Function(s) of server - begin */
/* init_thread - begin */
static void init_thread(void)
{
    Sem_init(&mutex, 0, 1);
    byte_cnt = 0;
}
/* init_thread - end */

/* thread - begin */
void *thread(void *vargp)
{
    int                     n;
    int                     connfd;
    char                    buf[MAXLINE];
    rio_t                   rio;
    static pthread_once_t   once = PTHREAD_ONCE_INIT;

    Pthread_detach(pthread_self());
    while (1)
    {
        connfd = sbuf_remove(&sbuf);
        Pthread_once(&once, init_thread);
        Rio_readinitb(&rio, connfd);

        while ((n = Rio_readlineb(&rio, buf, MAXLINE)) != 0)
        {
            P(&mutex);
            byte_cnt += n;
            printf("server received %d (%d total) bytes on fd %d\n", n, byte_cnt, connfd);
            //printf(" %s", buf);
            V(&mutex);
            if (!strcmp(buf, "exit\n"))
            {
                printf("Shutting down...\n");
                update_stock_file();
                free_stock_tree(stock_tree);
                exit(0);
            }
            service_client(buf, connfd);
        }
        Close(connfd);
    }
}
/* thread - end */

/* Service client - begin */
void service_client(char *buf, int connfd)
{
    int     stock_ID;
    int     num_dealing;
    char    msg[MAXLINE];

    if (!strcmp(buf, "show\n"))
    {
        memset(msg, 0, MAXLINE);
        read_stock_data(stock_tree, msg);
        Rio_writen(connfd, msg, MAXLINE);
        return;
    }

    if (!strncmp(buf, "buy", 3))
    {
        if (sscanf(buf, "buy %d %d", &stock_ID, &num_dealing) == 2)
            buy_stock(stock_tree, connfd, stock_ID, num_dealing);
        else
            Rio_writen(connfd, "Invalid command\n", MAXLINE);
        return;
    }

    if (!strncmp(buf, "sell", 4))
    {
        if (sscanf(buf, "sell %d %d", &stock_ID, &num_dealing) == 2)
            sell_stock(stock_tree, connfd, stock_ID, num_dealing);
        else
            Rio_writen(connfd, "Invalid command\n", MAXLINE);
        return;
    }

    Rio_writen(connfd, "Invalid command\n", MAXLINE);
}
/* Service client - end */
/* Function(s) of server - end */


/* Function(s) of stock data management - begin */
/* Initialization of stock data from stock.txt - begin */
Node *init_stock_data()
{
    int     stock_ID;
    int     stock_left;
    int     stock_price;
    FILE    *fp = fopen("stock.txt", "r");
    Node    *root = NULL;

    if (!fp)
    {
        fprintf(stderr, "stock.txt file open error\n");
        exit(1);
    }

    while (fscanf(fp, "%d %d %d", &stock_ID, &stock_left, &stock_price) > 0)
    {
        root = insert_node(root, stock_ID, stock_left, stock_price);
        if (!root)
        {
            fprintf(stderr, "Memory allocation error\n");
            exit(1);
        }
    }

    fclose(fp);
    return root;
}
/* Initialization of stock data from stock.txt - end */

/* Update stock.txt - begin */
void update_stock_file()
{
    FILE    *fp = fopen("stock.txt", "w");
    char    buf[MAXLINE];

    if (!fp)
    {
        fprintf(stderr, "stock.txt file open error\n");
        exit(1);
    }

    memset(buf, 0, MAXLINE);
    read_stock_data(stock_tree, buf);
    Fputs(buf, fp);
    fclose(fp);
}
/* Update stock.txt - end */

/* Insert node into binary tree - begin */
Node *insert_node(Node *root, int stock_ID, int stock_left, int stock_price)
{
    if (root == NULL)
    {
        root = (Node *)malloc(sizeof(Node));
        if (!root)
            return NULL;
        root->data.ID = stock_ID;
        root->data.stock_left = stock_left;
        root->data.price = stock_price;
        root->left = NULL;
        root->right = NULL;
        Sem_init(&root->data.mutex, 0, 1);
        Sem_init(&root->data.w, 0, 1);
        root->data.readcnt = 0;
        return root;
    }

    if (stock_ID > root->data.ID)
        root->right = insert_node(root->right, stock_ID, stock_left, stock_price);
    else if (stock_ID < root->data.ID)
        root->left = insert_node(root->left, stock_ID, stock_left, stock_price);
    
    return root;
}
/* Insert node into binary tree - end */

/* Buy stock - begin */
void buy_stock(Node *node, int connfd, int stock_ID, int num_dealing)
{
    if (!node)
    {
        Rio_writen(connfd, "no such stock\n", MAXLINE);
        return;
    }
    
    if (node->data.ID == stock_ID)
    {
        P(&node->data.w);
        if (node->data.stock_left < num_dealing)
        {
            Rio_writen(connfd, "Not enough left stocks\n", MAXLINE);
        }
        else
        {
            node->data.stock_left -= num_dealing;
            Rio_writen(connfd, "[buy]\033[0;32m success\033[0m\n", MAXLINE);
        }
        V(&node->data.w);
    }
    else if (node->data.ID < stock_ID)
        buy_stock(node->right, connfd, stock_ID, num_dealing);
    else
        buy_stock(node->left, connfd, stock_ID, num_dealing);
}
/* Buy stock - end */

/* Sell stock - begin */
void sell_stock(Node *node, int connfd, int stock_ID, int num_dealing)
{
    if (!node)
    {
        Rio_writen(connfd, "no such stock\n", MAXLINE);
        return;
    }
    
    if (node->data.ID == stock_ID)
    {
        P(&node->data.w);
        node->data.stock_left += num_dealing;
        Rio_writen(connfd, "[sell]\033[0;32m success\033[0m\n", MAXLINE);
        V(&node->data.w);
    }
    else if (node->data.ID < stock_ID)
        sell_stock(node->right, connfd, stock_ID, num_dealing);
    else
        sell_stock(node->left, connfd, stock_ID, num_dealing);
}
/* Sell stock - end */

/* Free stock tree - begin */
void free_stock_tree(Node *root)
{
    if (root == NULL)
        return;
    
    free_stock_tree(root->left);
    free_stock_tree(root->right);
    free(root);
}
/* Free stock tree - end */

/* Read stock data and write to buffer by inorder traversal - begin */
void read_stock_data(Node *root, char *buf)
{
    char temp[128];

    memset(temp, 0, 128);
    if (root != NULL)
    {
        read_stock_data(root->left, buf);
        P(&root->data.mutex);
        root->data.readcnt++;
        if (root->data.readcnt == 1)
            P(&root->data.w);
        V(&root->data.mutex);
        snprintf(temp, sizeof(temp), "%d %d %d\n", root->data.ID, root->data.stock_left, root->data.price);
        P(&root->data.mutex);
        root->data.readcnt--;
        if (root->data.readcnt == 0)
            V(&root->data.w);
        V(&root->data.mutex);
        strncat(buf, temp, MAXLINE - strlen(buf) - 1);
        read_stock_data(root->right, buf);
    }
}
/* Read stock data and write to buffer by inorder traversal - end */
/* Function(s) of stock data management - end */