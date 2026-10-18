#include "mycp.h"

#include <stdio.h>
#include <stdlib.h>

int main(int argc, char *argv[])
{
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <source> <destination>\n", argv[0]);
        return EXIT_FAILURE;
    }

    if (copy_path(argv[1], argv[2]) == -1) {
        fprintf(stderr, "Copy failed.\n");
        return EXIT_FAILURE;
    }

    printf("Copied successfully: %s -> %s\n", argv[1], argv[2]);
    return EXIT_SUCCESS;
}
