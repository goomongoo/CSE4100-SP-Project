#include "csapp.h"
#include "stockserver.h"

int     byte_cnt = 0;
Node    *stock_tree = NULL;

/* SIGINT handler - begin */
void sigint_handler(int sig)
{
    update_stock_file();
    free_stock_tree(stock_tree);
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
    static pool             pool;
    struct sockaddr_storage clientaddr;

    if (argc != 2)
    {
	    fprintf(stderr, "usage: %s <port>\n", argv[0]);
	    exit(0);
    }

    Signal(SIGINT, sigint_handler);
    stock_tree = init_stock_data();
    listenfd = Open_listenfd(argv[1]);
    init_pool(listenfd, &pool);

    while (1)
    {
        /* Wait for listening/connected descriptor(s) to become ready */
        pool.ready_set = pool.read_set;
        pool.nready = Select(pool.maxfd + 1, &pool.ready_set, NULL, NULL, NULL);

        /* If listening descriptor ready, add new client to pool */
        if (FD_ISSET(listenfd, &pool.ready_set))
        {
            clientlen = sizeof(struct sockaddr_storage);
            connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);
            add_client(connfd, &pool);
            Getnameinfo((SA *) &clientaddr, clientlen, client_hostname, MAXLINE, client_port, MAXLINE, 0);
            printf("Connected to (%s, %s)\n", client_hostname, client_port);
        }

        /* Echo a text line from each ready connected descriptor */
        check_clients(&pool);

        /* If no connected clients, shut down the server */
        /*if (pool.nclient == 0)
        {
            printf("No clients connected. Shutting down...\n");
            break;
        }*/
    }

    update_stock_file();
    free_stock_tree(stock_tree);
    return 0;
}
/* main - end */

/* Function(s) of server - begin */
/* Initialization of pool - begin */
void init_pool(int listenfd, pool *p)
{
    int i;

    /* Initially, there are no connected descriptors */
    p->maxi = -1;
    for (i = 0; i < FD_SETSIZE; i++)
        p->clientfd[i] = -1;

    /* Initially, listenfd is only member of select read set */
    p->maxfd = listenfd;
    FD_ZERO(&p->read_set);
    FD_SET(listenfd, &p->read_set);

    /* Initially, the number of connected clients is 0 */
    p->nclient = 0;
}
/* Initialization of pool - end */

/* Add client fd to pool - begin */
void add_client(int connfd, pool *p)
{
    int i;

    p->nready -= 1;
    for (i = 0; i < FD_SETSIZE; i++)    /* Find an available slot */
    {
        if (p->clientfd[i] < 0)
        {
            /* Add connected descriptor to the pool */
            p->clientfd[i] = connfd;
            Rio_readinitb(&p->clientrio[i], connfd);

            /* Add the descriptor to descriptor set */
            FD_SET(connfd, &p->read_set);

            /* Update max descriptor and pool highwater mark */
            if (p->maxfd < connfd)
                p->maxfd = connfd;
            if (p->maxi < i)
                p->maxi = i;
            
            /* Increment the number of connected clients */
            p->nclient++;

            break;
        }
    }
    if (i == FD_SETSIZE)
        app_error("add_client error: Too many clients");
}
/* Add client fd to pool - end */

/* Check client (read line and process) - begin */
void check_clients(pool *p)
{
    int     i;
    int     n;
    int     connfd;
    char    buf[MAXLINE];
    rio_t   rio;

    for (i = 0; (i <= p->maxi) && (p->nready > 0); i++)
    {
        connfd = p->clientfd[i];
        rio = p->clientrio[i];

        /* If the descriptor is ready, echo a text line from it */
        if ((connfd > 0) && (FD_ISSET(connfd, &p->ready_set)))
        {
            p->nready -= 1;
            if ((n = Rio_readlineb(&rio, buf, MAXLINE)) != 0)
            {
                byte_cnt += n;
                printf("Server received %d (%d total) bytes on fd %d\n", n, byte_cnt, connfd);
                //printf(" %s", buf);
                if (!strcmp(buf, "exit\n"))
                {
                    printf("Shutting down...\n");
                    update_stock_file();
                    free_stock_tree(stock_tree);
                    exit(0);
                }
                service_client(buf, connfd);
            }
            else /* EOF detected, remove descriptor from pool */
            {
                Close(connfd);
                FD_CLR(connfd, &p->read_set);
                p->clientfd[i] = -1;

                /* Decrement the number of connected clients */
                p->nclient--;
            }
        }
    }
}
/* Check client (read line and process) - end */

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
        if (node->data.stock_left < num_dealing)
        {
            Rio_writen(connfd, "Not enough left stocks\n", MAXLINE);
        }
        else
        {
            node->data.stock_left -= num_dealing;
            Rio_writen(connfd, "[buy]\033[0;32m success\033[0m\n", MAXLINE);
        }
        return;
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
        node->data.stock_left += num_dealing;
        Rio_writen(connfd, "[sell]\033[0;32m success\033[0m\n", MAXLINE);
        return;
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
        snprintf(temp, 128, "%d %d %d\n", root->data.ID, root->data.stock_left, root->data.price);
        strcat(buf, temp);
        read_stock_data(root->right, buf);
    }
}
/* Read stock data and write to buffer by inorder traversal - end */
/* Function(s) of stock data management - end */