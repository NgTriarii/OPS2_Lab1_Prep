#define _POSIX_C_SOURCE 200809L
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define MAX_KNIGHT_NAME_LENGTH 20

#define ERR(source) \
    (fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), perror(source), kill(0, SIGKILL), exit(EXIT_FAILURE))

typedef struct Knight
{
    char name[MAX_KNIGHT_NAME_LENGTH + 1];
    int hp;
    int atk;
    int id;
    int read_fd;
    int write_fd;
} knight_t;

int set_handler(void (*f)(int), int sig)
{
    struct sigaction act = {0};
    act.sa_handler = f;
    if (sigaction(sig, &act, NULL) == -1)
        return -1;
    return 0;
}

void msleep(int millisec)
{
    struct timespec tt;
    tt.tv_sec = millisec / 1000;
    tt.tv_nsec = (millisec % 1000) * 1000000;
    while (nanosleep(&tt, &tt) == -1)
    {
    }
}

int count_descriptors()
{
    int count = 0;
    DIR* dir;
    struct dirent* entry;
    struct stat stats;
    if ((dir = opendir("/proc/self/fd")) == NULL)
        ERR("opendir");
    char path[PATH_MAX];
    getcwd(path, PATH_MAX);
    chdir("/proc/self/fd");
    do
    {
        errno = 0;
        if ((entry = readdir(dir)) != NULL)
        {
            if (lstat(entry->d_name, &stats))
                ERR("lstat");
            if (!S_ISDIR(stats.st_mode))
                count++;
        }
    } while (entry != NULL);
    if (chdir(path))
        ERR("chdir");
    if (closedir(dir))
        ERR("closedir");
    return count - 1;  // one descriptor for open directory
}

void make_pipes(knight_t* knights, int n)
{
    for (int i = 0; i < n; i++)
    {
        int fd[2];
        pipe(fd);
        knights[i].read_fd = fd[0];
        knights[i].write_fd = fd[1];
    }
}

void child_work(knight_t* allies, knight_t* enemies, int allies_num, int enemies_num, int is_frank, int id)
{
    // knight_t* me = &allies[id];

    for (int i = 0; i < allies_num; i++)
    {
        if (i != id)
        {
            if (close(allies[i].read_fd))
            {
                ERR("close");
            }
            if (close(allies[i].write_fd))
            {
                ERR("close");
            }
        }
        else
        {
            if (close(allies[i].write_fd))
            {
                ERR("close");
            }
        }
    }

    printf("I am %s knight %s. I will serve my king with my %d HP and %d attack\n",
           (is_frank == 1 ? "Frankish" : "Spanish"), allies[id].name, allies[id].hp, allies[id].atk);

    for (int i = 0; i < enemies_num; i++)
    {
        if (close(enemies[i].write_fd))
        {
            ERR("close");
        }
    }

    if (close(allies[id].read_fd))
    {
        ERR("close");
    }

    free(allies);
    free(enemies);

    exit(EXIT_SUCCESS);
}

int main(int argc, char* argv[])
{
    srand(time(NULL));
    set_handler(SIG_IGN, SIGPIPE);

    FILE* sarac_f = fopen("saraceni.txt", "r+");
    if (sarac_f == NULL)
    {
        printf("Saracens have not arrived on the battlefield");
        return 0;
    }

    FILE* frank_f = fopen("franci.txt", "r+");
    if (frank_f == NULL)
    {
        printf("Franks have not arrived on the battlefield");
        return 0;
    }

    char frank_buff[MAX_KNIGHT_NAME_LENGTH + 32];
    char sarac_buff[MAX_KNIGHT_NAME_LENGTH + 32];
    fgets(sarac_buff, MAX_KNIGHT_NAME_LENGTH + 32, sarac_f);
    fgets(frank_buff, MAX_KNIGHT_NAME_LENGTH + 32, frank_f);

    int frank_num = atoi(frank_buff);

    int sarac_num = atoi(sarac_buff);

    knight_t* Franks = malloc(sizeof(knight_t) * frank_num);
    knight_t* Saracens = malloc(sizeof(knight_t) * sarac_num);

    for (int i = 0; i < frank_num; i++)
    {
        Franks[i].id = i;
        fscanf(frank_f, "%s %d %d", Franks[i].name, &Franks[i].hp, &Franks[i].atk);
    }

    for (int i = 0; i < sarac_num; i++)
    {
        Saracens[i].id = i;
        fscanf(sarac_f, "%s %d %d", Saracens[i].name, &Saracens[i].hp, &Saracens[i].atk);
    }

    fclose(frank_f);
    fclose(sarac_f);

    make_pipes(Franks, frank_num);
    make_pipes(Saracens, sarac_num);

    for (int i = 0; i < frank_num; i++)
    {
        pid_t pid = fork();
        if (pid == 0)
        {
            child_work(Franks, Saracens, frank_num, sarac_num, 1, i);
        }
        if (pid < 0)
        {
            ERR("fork");
        }
    }

    for (int i = 0; i < sarac_num; i++)
    {
        pid_t pid = fork();
        if (pid == 0)
        {
            child_work(Saracens, Franks, sarac_num, frank_num, 0, i);
        }
        if (pid < 0)
        {
            ERR("fork");
        }
    }

    for (int i = 0; i < frank_num; i++)
    {
        if (close(Franks[i].read_fd))
        {
            ERR("close");
        }
        if (close(Franks[i].write_fd))
        {
            ERR("close");
        }
    }

    for (int i = 0; i < sarac_num; i++)
    {
        if (close(Saracens[i].read_fd))
        {
            ERR("close");
        }
        if (close(Saracens[i].write_fd))
        {
            ERR("close");
        }
    }

    while (wait(NULL) > 0)
        ;
    free(Franks);
    free(Saracens);

    printf("Opened descriptors: %d\n", count_descriptors());
}


/* =========================================================================
 * Common 'errno' values for Pipe / FIFO operations
 * =========================================================================
 * * EPIPE   : Broken pipe. You tried to write to a pipe/FIFO that has no 
 * open readers. (Note: Usually generates a SIGPIPE signal too).
 * * EAGAIN  : Try again. You used a non-blocking fd (O_NONBLOCK) and either 
 * tried to read an empty pipe, or write to a full pipe. 
 * (Often synonymous with EWOULDBLOCK).
 * * EINTR   : Interrupted system call. A blocking read/write/open was 
 * interrupted by a signal handler before any data was processed.
 * * EBADF   : Bad file descriptor. The fd is invalid, closed, or you are 
 * using it wrong (e.g., trying to write to the read-end).
 * * EEXIST  : File exists. mkfifo() failed because the named pipe already 
 * exists at the specified path.
 * * ENOENT  : No such file or directory. You tried to open() a FIFO that 
 * hasn't been created yet.
 * * EMFILE  : Too many open files. pipe() or open() failed because your 
 * process has reached its limit for open file descriptors.
 * * EACCES  : Permission denied. You don't have the required read/write 
 * permissions for the FIFO file or its directory.
 * ========================================================================= */
