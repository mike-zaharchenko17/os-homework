#include <stdio.h>
#include <string.h>

#define MAX_NODES 128
#define MAX_NAME_LEN 64
#define CMD_LEN 1024
#define LINE_LEN  2048

typedef struct {
    char name[MAX_NAME_LEN];
    char command[CMD_LEN];
} Node;

static Node nodeArray[MAX_NODES];
static int node_count = 0;


// defines a buffered node externally so it's accessible and 'partializable'
extern Node bufferedNode;

int parseLine(char buffer[]) {
    char *key = strtok(buffer, "=");
    char *value = strtok(buffer, "\n");

    if (strcmp(key, "node") == 0) {
        strcpy(bufferedNode.name, value);
    }

    if (strcmp(key, "command") == 0) {
        strcpy(bufferedNode.command, value);
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

        if (bufferedNode.name[0] && bufferedNode.command[0]) {
            // We have both; finalize the node by copying to array and clearing the buffer
            nodeArray[node_count++] = bufferedNode;
            memset(&bufferedNode, 0, sizeof bufferedNode);
        }
    }


    return 0;
}