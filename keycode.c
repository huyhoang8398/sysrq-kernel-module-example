#include <stdio.h>

int sysrq_key_table_key2index(int key)
{
	int retval;

	if ((key >= '0') && (key <= '9'))
		retval = key - '0';
	else if ((key >= 'a') && (key <= 'z'))
		retval = key + 10 - 'a';
	else if ((key >= 'A') && (key <= 'Z'))
		retval = key + 36 - 'A';
	else
		retval = -1;
	return retval;
}

int main() {
	int res = sysrq_key_table_key2index(65);
	printf("%d\n", res);
	return 0;
}
