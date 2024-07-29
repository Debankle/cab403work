#include <stdio.h>

#define PI 3.14
#define circleArea(r) (PI * (r) * (r))

int main(int argc, char **argv) {
    float userInput = 0;
    for (int i = 0; i < 5; i++) {
        printf("\nEnter the radius of circle number %d - ", i+1);
        scanf("%f", &userInput);
        printf("The circle has an area of %.2f\n", circleArea(userInput));
    }

    return 0;
}