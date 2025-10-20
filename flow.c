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

int createNode() {
    Node n = {"Fuck", "you"};
}