#include <asm-generic/errno-base.h>
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

volatile sig_atomic_t stop_work;
static int read_fd;

void sig_handler(int signal){
    if(signal == SIGINT){
        stop_work = 1;
        close(read_fd);
    }
}

typedef struct node{
    int neighbors[MAX_GRAPH_NODES];
    int neighbor_num;
    int pipe[2];
} node_t;

typedef struct graph{
    node_t nodes[MAX_GRAPH_NODES];
    int node_num;
} graph_t;

typedef struct ant
{
    int ID;
    int path[MAX_PATH_LENGTH];
    int path_length;
}ant_t;

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

void child_work(node_t node, int id, int fd_w[MAX_GRAPH_NODES], int destination){
    srand(getpid());
    set_handler(sig_handler, SIGINT);
    printf("ID %d: ", id);
    for(int i = 0; i < node.neighbor_num; i++){
        printf("%d ", node.neighbors[i]);
    }
    printf("\n");
    read_fd = node.pipe[0];
    while(!stop_work){
        ant_t ant;
        if(read(node.pipe[0], &ant, sizeof(ant)) < 0){
            if(errno == EINTR || errno == EBADF){
                break;
            }
            ERR("read");
        }
        ant.path[ant.path_length++] = id;
        if(node.neighbor_num == 0 || ant.path_length >= MAX_PATH_LENGTH){
            printf("Ant {%d}: got lost\n", ant.ID);
        }
        else if(id == destination){
            printf("Ant {%d}: found food\n", ant.ID);
        }
        else{
            int next_node = rand() % node.neighbor_num;
            if(write(fd_w[node.neighbors[next_node]], &ant, sizeof(ant)) == -1){
                if(errno == EINTR){
                    break;
                }
                ERR("write");
            }
        }
        msleep(100);
    }
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
    int start = atoi(argv[2]);
    int destination = atoi(argv[3]);

    set_handler(SIG_IGN, SIGINT);
    set_handler(SIG_IGN, SIGPIPE);

    graph_t graph = read_colony(filename);

    for(int i = 0; i < graph.node_num; i++){
        if(pipe(graph.nodes[i].pipe) == -1){
            ERR("pipe");
        }
    }

    for(int i = 0; i < graph.node_num; i++){
        pid_t pid = fork();
        if(pid == -1){
            ERR("fork()");
        }
        else if(pid == 0){
            int fd_w[MAX_GRAPH_NODES];
            for(int j = 0; j < graph.node_num; j++){
                if(i == j){
                    close(graph.nodes[i].pipe[1]);
                }
                else{
                    close(graph.nodes[j].pipe[0]);
                    fd_w[j] = graph.nodes[j].pipe[1];
                }
            }
            child_work(graph.nodes[i], i, fd_w, destination);
            for(int j = 0; j < graph.node_num; j++){
                if(i == j){
                    close(graph.nodes[i].pipe[0]);
                }
                else{
                    close(fd_w[j]);
                }
            }
            exit(EXIT_SUCCESS);
        }
    }

    for(int i = 0; i < graph.node_num; i++){
        if(i == start){
            close(graph.nodes[i].pipe[0]);
        }
        else{
            close(graph.nodes[i].pipe[0]);
            close(graph.nodes[i].pipe[1]);
        }
    }
    
    int ant_index = 0;
    while(1){
        msleep(1000);
        ant_t ant = {};
        ant.ID = ant_index++;
        if(write(graph.nodes[start].pipe[1], &ant, sizeof(ant)) == -1){
            if(errno == EPIPE){
                break;
            }
            ERR("write");
        }
    }

    while(wait(NULL) > 0);

    close(graph.nodes[start].pipe[1]);

    exit(EXIT_SUCCESS);
}
