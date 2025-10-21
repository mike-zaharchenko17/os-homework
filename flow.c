#include <stdio.h>
#include <string.h>

#define MAX_NODES 128
#define MAX_PIPES 10
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

static Node nodeArray[MAX_NODES];
static int node_count = 0;


static Pipe pipeArray[MAX_PIPES];
static int pipe_count = 0;

// defines a buffered node externally so it's accessible and 'partializable'
static Node bufferedNode;
static Pipe bufferedPipe;

static int nodeIsComplete(const Node *nodePtr) {
    return (nodePtr->name[0] && nodePtr->command[0]);
}

static int pipeIsComplete(const Pipe *pipePtr) {
    return (pipePtr->name[0] && pipePtr->from[0] && pipePtr->to[0]);
}

static const Node* getNodeByName(const Node nodeArray[], int node_count, const char *name) {
    for (int i = 0; i < node_count; i++) {
        if (strcmp(nodeArray[i].name, name) == 0) {
            return &nodeArray[i];  // type: const Node*
        }
    }
    return NULL;
}

static const Pipe* getPipeByName(const Pipe pipeArray[], int pipe_count, const char *name) {
    for (int i = 0; i < pipe_count; i++) {
        if (strcmp(pipeArray[i].name, name) == 0) {
            return &pipeArray[i];  // const Pipe*
        }
    }
    return NULL;
}

int parseLine(char buffer[]) {
    char *key = strtok(buffer, "=");
    char *value = strtok(NULL, "\n");

    if (strcmp(key, "node") == 0) {
        strncpy(bufferedNode.name, value, MAX_NAME_LEN-1);
        bufferedNode.name[MAX_NAME_LEN-1] = '\0';
    }

    if (strcmp(key, "command") == 0) {
        strncpy(bufferedNode.command, value, CMD_LEN-1);
        bufferedNode.command[CMD_LEN-1] = '\0';
    }

    if (strcmp(key, "pipe") == 0) {
        strncpy(bufferedPipe.name, value, MAX_NAME_LEN-1);
        bufferedPipe.name[MAX_NAME_LEN-1] = '\0';
    }

    if (strcmp(key, "from") == 0) {
        strncpy(bufferedPipe.from, value, MAX_NAME_LEN-1);
        bufferedPipe.from[MAX_NAME_LEN-1] = '\0';
    }

    if (strcmp(key, "to") == 0) {
        strncpy(bufferedPipe.to, value, MAX_NAME_LEN-1);
        bufferedPipe.to[MAX_NAME_LEN-1] = '\0';
    }

    return 0;
}

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
        parseLine(buffer);

        if (nodeIsComplete(&bufferedNode)) {
            // we have both; finalize the node by copying to array and clearing the buffer
            if (node_count >= MAX_NODES) {
                fprintf(stderr,"too many nodes\n"); break; 
            }
            nodeArray[node_count] = bufferedNode;
            memset(&bufferedNode, 0, sizeof bufferedNode);
            node_count++;
        }

        if (pipeIsComplete(&bufferedPipe)) {
            if (pipe_count >= MAX_PIPES) { 
                fprintf(stderr,"too many pipes\n"); break; 
            }
            pipeArray[pipe_count] = bufferedPipe;
            memset(&bufferedPipe, 0, sizeof bufferedPipe);
            pipe_count++;
        }
    }

    fclose(f);

    if (argv[1] == NULL) {
        fprintf(stderr, "usage: <flowfile> %s <pipe_name>\n", argv[1]);
        return 1;
    }

    const char *pipeName = argv[2];

    printf("Pipe Name: %s", pipeName);

    const Pipe *targetPipe = getPipeByName(pipeArray, pipe_count, pipeName);

    if (targetPipe == NULL) {
        fprintf(stderr, "invalid pipe name");
        return 1;
    }

    const Node* fromNode = getNodeByName(nodeArray, node_count, targetPipe->from);
    const Node* toNode = getNodeByName(nodeArray, node_count, targetPipe->to);


    printf("Nodes: \n");
    for (int i = 0; i < node_count; i++) {
        Node n = nodeArray[i];
        printf("Node %d : { name: %s, cmd: %s }\n", i, n.name, n.command);
    }

    printf("\nPipes\n");
    for (int j = 0; j < pipe_count; j++) {
        Pipe p = pipeArray[j];
        printf("Pipe %d : { name: %s, from: %s, to: %s }\n", j, p.name, p.from, p.to);
    }

    printf("Target Pipe: { name: %s, from: %s, to: %s}\n", targetPipe->name, fromNode->name, toNode->name);

    return 0;
}