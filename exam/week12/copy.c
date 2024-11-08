#include <stdio.h>
#include <stdlib.h>


void copyFile(const char *srcPath, const char *destPath) {
    FILE *src, *dest;
    char buffer[1024];
    size_t bytesRead;

    src = fopen(srcPath, "rb");
    if (src == NULL) {
        perror("Error opening source file");
        exit(EXIT_FAILURE);
    }

    dest = fopen(destPath, "wb");
    if (dest == NULL) {
        perror("Error opening destination file");
        fclose(src);
        exit(EXIT_FAILURE);
    }

    while ((bytesRead = fread(buffer, 1, sizeof(buffer), src)) > 0) {
        fwrite(buffer, 1, bytesRead, dest);
    }

    fclose(src);
    fclose(dest);
}


int main(int argc, char **argv) {
    char *src = argv[1];
    char *dest = argv[2];

    copyFile(src, dest);

}