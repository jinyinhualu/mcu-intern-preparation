#include <stdio.h>

int main ()
{
	int a[] = {45, 21, 9, 65, 6, 125};
	int max = a[0], min = a[0];
	int* idx = a + 1;
	while (idx < a + sizeof (a) / sizeof (a[0]))
	{
		idx ++;
		if (max < *idx) max = *idx;
		if (min > *idx) min = *idx;
	}

	printf ("max = %d, min = %d", max, min);
	return 0;
}