#include <stdio.h>
#include <string.h>

typedef struct Person {
	char name[50];
	int citNo;
	float salary;
} Person_t;

int main(int argc, char **argv) {
	Person_t person1;
	strcpy(person1.name, "George");
	person1.citNo = 1984;
	person1.salary = 2500;

	printf("\nName: %s\n", person1.name);
	printf("Citizenship No.: %d\n", person1.citNo);
	printf("Salary: %.2f\n", person1.salary);

	Person_t person2;
	printf("\nEnter the person's name: ");
	scanf("%49s", person2.name);
	printf("Enter the person's citizenship number: ");
	scanf("%d", &person2.citNo);
	printf("Enter the person's salary: ");
	scanf("%f", &person2.salary);

	printf("\nName: %s\n", person2.name);
	printf("Citizenship No.: %d\n", person2.citNo);
	printf("Salary: %.2f\n", person2.salary);

	return 0;
}
