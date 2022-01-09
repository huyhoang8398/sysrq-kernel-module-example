#include <assert.h>
#include <fcntl.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
const char *tmp = "pressed";
int state = 0;
char *cmp_addr;

void * memmem(const void * haystack, size_t n,
		const void * needle, size_t m) {
	const unsigned char * y = (const unsigned char * ) haystack;
	const unsigned char * x = (const unsigned char * ) needle;

	size_t j, k, l;

	if (m > n || !m || !n)
		return NULL;

	if (1 != m) {
		if (x[0] == x[1]) {
			k = 2;
			l = 1;
		} else {
			k = 1;
			l = 2;
		}

		j = 0;
		while (j <= n - m) {
			if (x[1] != y[j + 1]) {
				j += k;
			} else {
				if (!memcmp(x + 2, y + j + 2, m - 2) &&
						x[0] == y[j])
					return (void * ) & y[j];
				j += l;
			}
		}
	} else
		do {
			if ( * y == * x)
				return (void * ) y;
			y++;
		} while (--n);

	return NULL;
}

int main() {
	char kernel_val[15];
	int fd, ret, n;
	struct pollfd pfd;

	fd = open("/dev/lkm_a_key", O_RDONLY | O_NONBLOCK);
	if( fd == -1 ) {
		perror("open");
		exit(EXIT_FAILURE);
	}

	pfd.fd = fd;
	pfd.events = ( POLLIN | POLLRDNORM | POLLOUT | POLLWRNORM );

	while( 1 ) {
		puts("Starting poll...");

		ret = poll(&pfd, (unsigned long)1, 5000);   //wait for 5secs

		if( ret < 0 ) {
			perror("poll");
			assert(0);
		}

		if( ( pfd.revents & POLLIN )  == POLLIN ) {
			read(pfd.fd, &kernel_val, sizeof(kernel_val));
			//TODO: thay hello world = empty
			//compare if value is not empty -> print
			cmp_addr = memmem(kernel_val, 15, tmp, 7);
			if ((cmp_addr != NULL) && state == 0) {
				// printf("%s\n", buff);
				printf("POLLIN : Kernel_val = %s\n", kernel_val);
				state = 1;
			} else if (cmp_addr != NULL)
				state = 0;
		}

	}
	return 0;

}
