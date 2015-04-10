#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>

unsigned long
getTick ()
{
	static unsigned long init = 0;
	unsigned long t;
	struct timespec ts;

	clock_gettime(CLOCK_REALTIME, &ts);
	t = ts.tv_nsec / 1000000;
	t += ts.tv_sec * 1000;
	if (init == 0)
		init = t;
	return t - init;
}

int main(int argc, char *argv[])
{
	if (argc > 1 && !strcmp(argv[1], "wait")) {
		long int ms = 1000;
		const char *f = NULL;
		if (argc > 2)
			ms = atol(argv[2]);
		if (argc > 3)
			nice(atoi(argv[3]));
		if (argc > 4)
			f = argv[4][0] ? argv[4] : NULL;
		ms += getTick();
		while (ms > getTick())
			if (f && !access(f, R_OK))
				break;
	}
	return 0;
}
