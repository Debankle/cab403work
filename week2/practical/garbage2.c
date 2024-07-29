#include <stdio.h>

int main() {
    // This is purely illustrative and will not work on modern systems.
    // Direct memory access example for old systems like DOS.

    unsigned short *videoMemory = (unsigned short *)0xB8000;

    // Write some values to video memory
    for (int i = 0; i < 80 * 25; i++) {      // 80 columns x 25 rows
        videoMemory[i] = (0x1F << 8) | 'X';  // Blue foreground, 'X' character
    }

    printf("Written to video memory.\n");
    return 0;
}
