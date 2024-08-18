#include <stdlib.h>
#include <ctype.h>
#include <stdio.h>
#include <time.h>
#include <errno.h>
#include <limits.h>
#include <string.h>

/**
 * Function to generate n random numbers between a lower and upper range
 * \param lowerRange - lowest value of random number
 * \param upperRange - largest value of random number
 * \param count - number of random numbers to generate
 */
int *genRandomIntegerNumbers(int lowerRange, int upperRange, int count) 
{ 
    srand(time(NULL));
    int *numbers = (int *)malloc(count * sizeof(int));
    if (numbers == NULL) {
        perror("malloc");
        return NULL;
    }

    srand(time(NULL));
    for (int i = 0; i < count; i++) {
        numbers[i] = rand() % (upperRange - lowerRange + 1) + lowerRange;
    }

    return numbers;
} 

/**
 * Function to check that the name provided for shared memory object has a leading '/'
 * \param name - name provided for shared memory object
 * \returns 1 if invalid else 0
 */
int checkShMemName(char *name) {
  if ((name[0]!='/') || (strlen(name)==1)) {
    return 1;
  }
  return 0;
}

/**
 * Function to check char array contains only integers as represented by each char
 * \param value[] - array of chars
 * \return 0 true else 1 false
 */
int isNumber(char value[]) {
    int i;   
    for (i=0; value[i]!='\0';i++) {
        if (isdigit(value[i])==0) {
            return 1;
        }
    }
    return 0;
}

int charToInt(char *value, int *out) {
    char *end;
    errno = 0;
    long val = strtol(value, &end, 10);

    if (end == value) {
        return 0;
    }

    if (*end != '\0') {
        return 0;
    }

    if (errno == ERANGE && (val == LONG_MAX || val == LONG_MIN)) {
        return 0;
    }

    if (val > INT_MAX || val < INT_MIN) {
        return 0;
    }

    *out = (int)val;
    return 1;
}

/**
 * Comparison functions for qsort
 * compareA with sort values into non-descending order
 * compareB will sort values into non-ascending order
 */
int compareA(const void *val1, const void *val2) {
  return (*(int*)val1 - *(int*)(val2));
}

int compareB(const void *val1, const void *val2) {
  return (*(int*)val2 - *(int*)(val1));
}

/**
 * Print out an array of numbers
 * \param arrayValues - the array of values to print
 * \param length - length of array
 */
void printArray(const char *narr, int *arr, int length) {
  printf("\nValues in the array\n%s ", narr);
  for (int i=0; i < length; i++) {
    printf("%d ", arr[i]);    
  }
  printf("\n");
}