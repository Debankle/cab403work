#include <stdio.h>

int x = 1;

int add(int a, int b) {
	return a + b;
}

void cringe() {
	static int y = 0;
	x += y;
	y++;
}

int main() {
	cringe();
	int a = 1;
	int b = 2;
	int c = add(a,b);
	printf("%d\n", c);
	cringe();
	int d = add(x,a);
	printf("%d\n",d);
	cringe();
	d = add(x,a);
	printf("%d\n", d);
	return 0;
}
