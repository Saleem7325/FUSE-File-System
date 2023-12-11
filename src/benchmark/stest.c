#include <stdlib.h>
#include <stdio.h>
#include <string.h>

int main(int argc, char **argv) {
    char p1[50] = "/this/is/a/path/";
    char path1[50];

    memcpy(&path1, &p1, 50);
    printf("Path: %s\n", p1);

    char *token = strtok(path1, "/");
    // printf("Path: %s\n", path1);

    while(token != NULL) {
        printf( "%s len: %ld\n", token, strlen(token));
        token = strtok(NULL, "/");
    }

    // char p2[50] = "/path";
    // char path2[50];

    // memcpy(&path2, &p2, 50);
    // printf("\nPath: %s\n", p2);

    // token = strtok(path2, "/");
    // // printf("Path: %s\n", path2);
    
    // while(token != NULL) {
    //     printf( "%s\n", token );
    //     token = strtok(NULL, "/");
    // }

    // char *ptr = malloc(7);
    // char string[] = "string";
    // memcpy(ptr, &string, strlen(string));
    // printf("\nString: %ld\n", strlen(ptr));

}