#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <libgen.h>

// char *get_parent(char *path){
//     if(!path || strcmp(path, "/") == 0){
//         return "/";
//     }

//     int len = strlen(path);

//     for(int i = len - 1; i >= 0; i++){
//         if(cpy[i] != '/'){
//             cpy[i] = '';
//         }else{
//             break;
//         }
//     }

//     char cpy[len];
//     printf("\nParent: %s\n", cpy);
// }

// char *get_file_w_path(char *){

// }

int main(int argc, char **argv) {
    char p1[50] = "/this/is/a/path/";
    char path1[50];

    memcpy(&path1, &p1, 50);
    printf("Path: %s\n", p1);

    char *token = strtok(path1, "/");
    printf("Path: %s\n", path1);

    while(token != NULL) {
        printf( "%s\n", token);
        token = strtok(NULL, "/");
    }

    char *pa1 = malloc(17);
    char str[17] = "/this/is/a/path/";
    memcpy(pa1, str, 17);


    char *parent = dirname(pa1);
    char *file = basename(str);
    printf("\nParent: %s\nBasename: %s\n", parent, file);
    free(pa1);

    char *pa2 = malloc(6);
    char str1[6] = "/file";
    memcpy(pa2, str, 6);

    char *parent1 = dirname(pa2);
    char *file1 = basename(str1);
    printf("\nParent: %s\nBasename: %s\n", parent1, file1);
    free(pa1);
    return 0;
}