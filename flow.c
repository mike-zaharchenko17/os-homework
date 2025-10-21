#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>

#define MAX_NODES 128
#define MAX_PIPES 10
#define MAX_CONCATS 64
#define MAX_PARTS 32
#define MAX_NAME_LEN 64
#define CMD_LEN 1024
#define LINE_LEN  2048

typedef struct {
    char name[MAX_NAME_LEN];
    char command[CMD_LEN];
} Node;

typedef struct {
    char name[MAX_NAME_LEN];
    char from[MAX_NAME_LEN];
    char to[MAX_NAME_LEN];
} Pipe;

typedef struct {
    char name[MAX_NAME_LEN];
    int parts;
    char part_name[MAX_PARTS][MAX_NAME_LEN];
} Concat;

typedef struct {
    char name[MAX_NAME_LEN];
    char from_node[MAX_NAME_LEN];
} Stderr;

static Node node_array[MAX_NODES];
static int node_count = 0;

static Pipe pipe_array[MAX_PIPES];
static int pipe_count = 0;

static Concat concat_array[MAX_CONCATS];
static int concat_count = 0;

static Stderr stderr_array[MAX_CONCATS];
static int stderr_count = 0;

// defines a buffered node externally so it's accessible and 'partializable'
static Node buffered_node;
static Pipe buffered_pipe;
static Concat buffered_concat;
static Stderr buffered_stderr;
static int expected_parts = 0;
static int collected_parts = 0;

/* HELPERS */
static int node_is_complete(const Node *nodePtr) {
    return (nodePtr->name[0] && nodePtr->command[0]);
}

static int pipe_is_complete(const Pipe *pipePtr) {
    return (pipePtr->name[0] && pipePtr->from[0] && pipePtr->to[0]);
}

static int concat_is_complete(const Concat *concatPtr) {
    return (concatPtr->name[0] && concatPtr->parts > 0);
}

static int stderr_is_complete(const Stderr *stderrPtr) {
    return (stderrPtr->name[0] && stderrPtr->from_node[0]);
}

static const Node* get_node_by_name(const Node node_array[], int node_count, const char *name) {
    for (int i = 0; i < node_count; i++) {
        if (strcmp(node_array[i].name, name) == 0) {
            return &node_array[i];  // type: const Node*
        }
    }
    return NULL;
}

static const Pipe* get_pipe_by_name(const Pipe pipe_array[], int pipe_count, const char *name) {
    for (int i = 0; i < pipe_count; i++) {
        if (strcmp(pipe_array[i].name, name) == 0) {
            return &pipe_array[i];  // const Pipe*
        }
    }
    return NULL;
}

static const Concat* get_concat_by_name(const Concat concat_array[], int concat_count, const char *name) {
    for (int i = 0; i < concat_count; i++) {
        if (strcmp(concat_array[i].name, name) == 0) {
            return &concat_array[i];
        }
    }
    return NULL;
}

static const Stderr* get_stderr_by_name(const Stderr stderr_array[], int stderr_count, const char *name) {
    for (int i = 0; i < stderr_count; i++) {
        if (strcmp(stderr_array[i].name, name) == 0) {
            return &stderr_array[i];
        }
    }
    return NULL;
}

/* I/O */

int parse_line(char buffer[]) {
    char *key = strtok(buffer, "=");
    char *value = strtok(NULL, "\n");

    if (strcmp(key, "node") == 0) {
        strncpy(buffered_node.name, value, MAX_NAME_LEN-1);
        buffered_node.name[MAX_NAME_LEN-1] = '\0';
    }

    if (strcmp(key, "command") == 0) {
        strncpy(buffered_node.command, value, CMD_LEN-1);
        buffered_node.command[CMD_LEN-1] = '\0';
    }

    if (strcmp(key, "pipe") == 0) {
        strncpy(buffered_pipe.name, value, MAX_NAME_LEN-1);
        buffered_pipe.name[MAX_NAME_LEN-1] = '\0';
    }

    if (strcmp(key, "from") == 0) {
        strncpy(buffered_pipe.from, value, MAX_NAME_LEN-1);
        buffered_pipe.from[MAX_NAME_LEN-1] = '\0';
    }

    if (strcmp(key, "to") == 0) {
        strncpy(buffered_pipe.to, value, MAX_NAME_LEN-1);
        buffered_pipe.to[MAX_NAME_LEN-1] = '\0';
    }

    // Handle stderr directive
    if (strcmp(key, "stderr") == 0) {
        memset(&buffered_stderr, 0, sizeof buffered_stderr);
        strncpy(buffered_stderr.name, value, MAX_NAME_LEN-1);
        buffered_stderr.name[MAX_NAME_LEN-1] = '\0';
    }

    if (strcmp(key, "from") == 0 && buffered_stderr.name[0]) {
        strncpy(buffered_stderr.from_node, value, MAX_NAME_LEN-1);
        buffered_stderr.from_node[MAX_NAME_LEN-1] = '\0';
    }

    // Handle concatenate directive
    if (strcmp(key, "concatenate") == 0) {
        memset(&buffered_concat, 0, sizeof buffered_concat);
        strncpy(buffered_concat.name, value, MAX_NAME_LEN-1);
        buffered_concat.name[MAX_NAME_LEN-1] = '\0';
        expected_parts = 0;
        collected_parts = 0;
    }

    if (strcmp(key, "parts") == 0 && buffered_concat.name[0]) {
        expected_parts = atoi(value);
    }

    if (strncmp(key, "part_", 5) == 0 && buffered_concat.name[0]) {
        int idx = atoi(key + 5);
        if (idx >= 0 && idx < MAX_PARTS) {
            strncpy(buffered_concat.part_name[idx], value, MAX_NAME_LEN-1);
            buffered_concat.part_name[idx][MAX_NAME_LEN-1] = '\0';
            collected_parts++;
        }
    }

    return 0;
}

/* PROCESS CREATOR / EXECUTOR */

int run_pipe(const char *leftCmd, const char *rightCmd) {
    // fd[0] - read end of the pipe
    // fd[1] - write end of the pipe
    int fd[2];

    if (pipe(fd) <  0) { 
        perror("error on pipe"); 
        return -1; 
    }

    pid_t c1 = fork();

    // if it's the child process, make this one the producer
    if (c1 == -1) { 
        perror("error on c1 fork"); 
        close(fd[0]); 
        close(fd[1]); 
        return -1; 
    }

    if (c1 == 0) {
        // redirect the stdout of this process into the write end of the pipe
        dup2(fd[1], STDOUT_FILENO);
        close(fd[0]); // don't need read end
        close(fd[1]); // close original write end

        // run as shell- lets us not have to write a parser for this
        execlp("sh", "sh", "-c", leftCmd, (char*)0);
        perror("error executing left command");
        _exit(127);
    }

    pid_t c2 = fork();

    if (c2 == -1) { 
        perror("error on c2 fork"); 
        close(fd[0]); 
        close(fd[1]); 
        return -1; 
    }
    
    // and this one the consumer
    if (c2 == 0) {
        // redirect the stdin of this process into the read end of the pipe
        // i.e., this will read from the pipe
        dup2(fd[0], STDIN_FILENO);
        close(fd[1]); // ditto above
        close(fd[0]);
        execlp("sh", "sh", "-c", rightCmd, (char*)0);
        perror("error executing right command");
        _exit(127);
    }

    close(fd[0]);
    close(fd[1]);

    int status;

    waitpid(c1, NULL, 0);
    waitpid(c2, &status, 0);

    if (WIFEXITED(status)) {
        return WEXITSTATUS(status);
    }

    return -1;
}

// Run a node so that its STDOUT goes to out_fd
static int run_node_into_fd(const Node *n, int out_fd) {
    pid_t c = fork();
    if (c == 0) {
        if (dup2(out_fd, STDOUT_FILENO) < 0) { 
            perror("dup2"); 
            _exit(127); 
        }
        execlp("sh", "sh", "-c", n->command, (char*)0);
        perror("exec node");
        _exit(127);
    }
    int st=0; waitpid(c,&st,0);
    return (WIFEXITED(st)? WEXITSTATUS(st) : -1);
}

// Run STDERR of a node into out_fd (and silence its normal stdout)
static int run_stderr_into_fd(const Stderr *sd, int out_fd) {
    const Node *n = get_node_by_name(node_array, node_count, sd->from_node);
    if (!n) { 
        fprintf(stderr,"stderr from unknown node '%s'\n", sd->from_node); 
        return -1; 
    }

    pid_t c = fork();
    if (c == 0) {
        // redirect this child's STDERR to out_fd
        if (dup2(out_fd, STDERR_FILENO) < 0) {
             perror("dup2 err"); 
             _exit(127); 
        }

        // silence its STDOUT so it doesn't mix with errors
        int devnull = open("/dev/null", O_WRONLY);
        if (devnull >= 0) {
            dup2(devnull, STDOUT_FILENO);
            close(devnull);
        }

        execlp("sh", "sh", "-c", n->command, (char*)0);
        perror("exec stderr-from");
        _exit(127);
    }
    int st=0; waitpid(c,&st,0);
    return (WIFEXITED(st)? WEXITSTATUS(st) : -1);
}

// Run a pipe and send the RIGHT side's STDOUT into out_fd
static int run_pipe_into_fd(const Pipe *p, int out_fd) {
    // set up the inner ls|wc, but make wc's stdout go to out_fd
    int fd[2]; if (pipe(fd) < 0) { 
        perror("pipe"); 
        return -1; 
    }

    const Node *from = get_node_by_name(node_array, node_count, p->from);
    const Node *to = get_node_by_name(node_array, node_count, p->to);

    if (!from || !to) { 
        fprintf(stderr,"bad pipe endpoints\n"); 
        close(fd[0]); 
        close(fd[1]); 
        return -1; 
    }

    pid_t c1 = fork();
    if (c1 == 0) { // producer
        dup2(fd[1], STDOUT_FILENO);
        close(fd[0]); close(fd[1]);
        execlp("sh","sh","-c", from->command, (char*)0);
        perror("exec left"); _exit(127);
    }

    pid_t c2 = fork();
    if (c2 == 0) { // consumer
        dup2(fd[0], STDIN_FILENO);
        close(fd[1]); close(fd[0]);
        // redirect consumer's stdout to out_fd
        if (dup2(out_fd, STDOUT_FILENO) < 0) { perror("dup2 out"); _exit(127); }
        execlp("sh","sh","-c", to->command, (char*)0);
        perror("exec right"); _exit(127);
    }

    close(fd[0]); close(fd[1]);
    int st; waitpid(c1,NULL,0); waitpid(c2,&st,0);
    return (WIFEXITED(st)? WEXITSTATUS(st) : -1);
}

// run a concatenate by sequentially running each part into out_fd
static int run_concat_into_fd(const Concat *c, int out_fd) {
    for (int i = 0; i < c->parts; i++) {
        const char *name = c->part_name[i];
        if (!name[0]) continue; // skip holes if parts came out-of-order in file

        // dispatch by kind (node, pipe, concatenate, stderr)
        if (get_node_by_name(node_array, node_count, name)) {
            int rc = run_node_into_fd(get_node_by_name(node_array, node_count, name), out_fd);
            if (rc != 0) return rc;
        } else if (get_pipe_by_name(pipe_array, pipe_count, name)) {
            int rc = run_pipe_into_fd(get_pipe_by_name(pipe_array, pipe_count, name), out_fd);
            if (rc != 0) return rc;
        } else if (get_concat_by_name(concat_array, concat_count, name)) {
            int rc = run_concat_into_fd(get_concat_by_name(concat_array, concat_count, name), out_fd);
            if (rc != 0) return rc;
        } else if (get_stderr_by_name(stderr_array, stderr_count, name)) {
            int rc = run_stderr_into_fd(get_stderr_by_name(stderr_array, stderr_count, name), out_fd);
            if (rc != 0) return rc;
        } else {
            fprintf(stderr,"unknown component '%s' in concatenate '%s'\n", name, c->name);
            return -1;
        }
    }
    return 0;
}

/* DRIVER */

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "usage: %s <flowfile>\n", argv[0]);
        return 1;
    }

    // open .flow
    FILE *f = fopen(argv[1], "r");

    if (f == NULL) {
        fprintf(stderr, "unable to open file");
        return 1;
    }

    char buffer[LINE_LEN];

    while (fgets(buffer, LINE_LEN, f)) {
        parse_line(buffer);

        if (node_is_complete(&buffered_node)) {
            // we have both; finalize the node by copying to array and clearing the buffer
            if (node_count >= MAX_NODES) {
                fprintf(stderr,"too many nodes\n"); break; 
            }
            node_array[node_count] = buffered_node;
            memset(&buffered_node, 0, sizeof buffered_node);
            node_count++;
        }

        if (pipe_is_complete(&buffered_pipe)) {
            if (pipe_count >= MAX_PIPES) { 
                fprintf(stderr,"too many pipes\n"); break; 
            }
            pipe_array[pipe_count] = buffered_pipe;
            memset(&buffered_pipe, 0, sizeof buffered_pipe);
            pipe_count++;
        }

        // finalize stderr
        if (stderr_is_complete(&buffered_stderr)) {
            if (stderr_count < MAX_CONCATS) {
                stderr_array[stderr_count++] = buffered_stderr;
                memset(&buffered_stderr, 0, sizeof buffered_stderr);
            }
        }

        // finalize concatenate
        if (buffered_concat.name[0] && expected_parts > 0 && collected_parts >= expected_parts) {
            buffered_concat.parts = expected_parts;
            if (concat_count < MAX_CONCATS) {
                concat_array[concat_count++] = buffered_concat;
                memset(&buffered_concat, 0, sizeof buffered_concat);
                expected_parts = collected_parts = 0;
            }
        }
    }

    fclose(f);

    if (argv[1] == NULL) {
        fprintf(stderr, "usage: <flowfile> %s <pipe_name>\n", argv[1]);
        return 1;
    }

    const char *pipeName = argv[2];

    printf("Pipe Name: %s", pipeName);

    const Pipe *target_pipe = get_pipe_by_name(pipe_array, pipe_count, pipeName);

    if (target_pipe == NULL) {
        fprintf(stderr, "invalid pipe name");
        return 1;
    }

    const Node* to_node = get_node_by_name(node_array, node_count, target_pipe->to);
    if (!to_node) {
        fprintf(stderr, "unknown destination node '%s'\n", target_pipe->to);
        return 1;
    }

    // create pipe for the connection
    int fd[2];
    if (pipe(fd) < 0) {
        perror("pipe");
        return 1;
    }

    // Launch the consumer (to node) first
    pid_t consumer = fork();
    if (consumer == 0) {
        dup2(fd[0], STDIN_FILENO);
        close(fd[1]); close(fd[0]);
        execlp("sh","sh","-c", to_node->command, (char*)0);
        perror("exec consumer");
        _exit(127);
    }

    // parent will produce into fd[1] using the dispatcher
    close(fd[0]);                // parent keeps only write end
    int out_fd = fd[1];

    // decide what "from" is and stream it into out_fd
    int rc = -1;
    const char *from_name = target_pipe->from;
    
    if (get_node_by_name(node_array, node_count, from_name)) {
        rc = run_node_into_fd(get_node_by_name(node_array, node_count, from_name), out_fd);
    } else if (get_pipe_by_name(pipe_array, pipe_count, from_name)) {
        rc = run_pipe_into_fd(get_pipe_by_name(pipe_array, pipe_count, from_name), out_fd);
    } else if (get_concat_by_name(concat_array, concat_count, from_name)) {
        rc = run_concat_into_fd(get_concat_by_name(concat_array, concat_count, from_name), out_fd);
    } else if (get_stderr_by_name(stderr_array, stderr_count, from_name)) {
        rc = run_stderr_into_fd(get_stderr_by_name(stderr_array, stderr_count, from_name), out_fd);
    } else {
        fprintf(stderr,"unknown source '%s'\n", from_name);
        rc = -1;
    }

    // close write end so consumer sees EOF
    close(out_fd);

    int st=0; waitpid(consumer, &st, 0);
    if (rc != 0) return rc;
    return (WIFEXITED(st)? WEXITSTATUS(st) : -1);
}