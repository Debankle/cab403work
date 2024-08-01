#include <stdio.h>
#include <stdlib.h>

void enterValuesIntoArr(int *prt, int length);
void printOutArray(int *ptr, int length);
int askUserToIncreaseArraySize(int *ptr, int length);
void enterValuesIntoExistingArray(int *ptr, int length, int oldLength);

int main(int argc, char **argv) {
	int numElements = 0;
	int *arrOnePtr;

	printf("\nPlease enter the number you want the initial array to have: ");
	scanf("%d", &numElements);
	arrOnePtr = (int*) malloc(sizeof(int) * numElements);
	if (arrOnePtr == NULL) {
		perror("\nFailed to allocate memory");
		return -1;
	}

	enterValuesIntoArr(arrOnePtr, numElements);
	printOutArray(arrOnePtr,numElements);

	int newSize = askUserToIncreaseArraySize(arrOnePtr, numElements);

	enterValuesIntoExistingArray(arrOnePtr, numElements, numElements+newSize);
	printOutArray(arrOnePtr, numElements+newSize);

	return 0;
}

void enterValuesIntoExistingArray(int *ptr, int length, int oldLength) {
	printf("\n\nPlease assign new values to the longer array:");
	int element;
	for (size_t i = oldLength; i < length; i++) {
		printf("\nEnter value of %d element: ", i+1);
		scanf("%d", &element);
		ptr[i] = element;
	}
}

int askUserToIncreaseArraySize(int *ptr, int length) {
	int additionalElements = 0;
	printf("How much should the array be extended by? ");
	scanf("%d", additionalElements);

	ptr = (int*)realloc(ptr, (length + additionalElements) * sizeof(int));
	if (ptr == NULL) {
		perror("\nFailed to reallocate memory.");
		return -1;
	}

	return additionalElements;
}

void enterValuesIntoArr(int *ptr, int length) {
	printf("\nPlease assign values into the array:");
	int element;
	for (size_t i = 0; i < length; i++) {
		printf("\nEnter value of %d element: ", i+1);
		scanf("%d", &element);
		ptr[i] = element;
	}
}

void printOutArray(int *ptr, int length) {
	printf("\nPrinting the values in the array:\n");
	for (int i = 0; i < length; i++) {
		printf("%d ", ptr[i]);
	}
}