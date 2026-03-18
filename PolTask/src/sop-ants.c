#define _POSIX_C_SOURCE 200809L

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define ERR(source) \
    (fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), perror(source), kill(0, SIGKILL), exit(EXIT_FAILURE))

#define MAX_GRAPH_NODES 32
#define MAX_PATH_LENGTH (2 * MAX_GRAPH_NODES)

#define FIFO_NAME "/tmp/colony_fifo"

typedef struct node{
    int neighbors[MAX_GRAPH_NODES];
    int neighbor_num;
    int pipe[2];
} node_t;

typedef struct graph{
    node_t nodes[MAX_GRAPH_NODES];
    int node_num;
} graph_t;

int set_handler(void (*f)(int), int sig)
{
    struct sigaction act = {0};
    act.sa_handler = f;
    if (sigaction(sig, &act, NULL) == -1)
        return -1;
    return 0;
}

void msleep(int ms)
{
    struct timespec tt;
    tt.tv_sec = ms / 1000;
    tt.tv_nsec = (ms % 1000) * 1000000;
    while (nanosleep(&tt, &tt) == -1)
    {
    }
}

void usage(int argc, char* argv[])
{
    printf("%s graph start dest\n", argv[0]);
    printf("  graph - path to file containing colony graph\n");
    printf("  start - starting node index\n");
    printf("  dest - destination node index\n");
    exit(EXIT_FAILURE);
}

void child_work(node_t node, int id){
    printf("ID %d: ", id);
    if (node.neighbor_num <= 0) {
        printf("None\n");
        return;
    }
    for(int i = 0; i < node.neighbor_num - 1; i++){
        printf("%d ", node.neighbors[i]);
    }
    printf("%d\n", node.neighbors[node.neighbor_num - 1]);
}

graph_t read_colony(char* filename){

    FILE* fd = fopen(filename, "r+");

    if(fd == NULL){
        ERR("fopen");
    }

    graph_t graph = {0};
    if(fscanf(fd, "%d", &graph.node_num) != 1){
        ERR("fscanf");
    }

    while(1){
        int from;
        int to;
        if(fscanf(fd, "%d %d", &from, &to) != 2){
            break;
        }
        node_t* node_ptr = &graph.nodes[from];
        node_ptr->neighbors[node_ptr->neighbor_num++] = to;
    }

    fclose(fd);

    return graph;
    
}

int main(int argc, char* argv[])
{
    if (argc != 4)
        usage(argc, argv);


    char* filename = argv[1];

    graph_t graph = read_colony(filename);

    for(int i = 0; i < graph.node_num; i++){
        pid_t pid = fork();
        if(pid == -1){
            ERR("fork()");
        }
        else if(pid == 0){
            child_work(graph.nodes[i], i);
            exit(EXIT_SUCCESS);
        }
    }

    while(wait(NULL) > 0);

    exit(EXIT_SUCCESS);
}
