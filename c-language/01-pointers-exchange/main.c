#include <stdio.h>

void Exchange (int* a, int* b);

int main ()
{
	int a = 2;
	int b = 3;
	Exchange(&a, &b);
	printf ("%d, %d", a, b);
}

void Exchange (int* a, int* b)
{
	int c = *a;
	*a = *b;
	*b = c;
}
	