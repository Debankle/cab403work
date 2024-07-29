#include <stdio.h>

int main() {
	FILE *fp = fopen("fileTest.q","wb");
	fprintf(fp, "Hello\n");
	fclose(fp);
	return 0;
}
