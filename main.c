#include "ciln.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>
#include <ciln.h>

#ifndef CILN_CC
# define CILN_CC "cc"
#endif

#ifndef CILN_DIR
# define CILN_DIR "./.ciln"
#endif

#ifndef CILN_OUTPUT
# define CILN_OUTPUT CILN_DIR "/build"
#endif

int main(int argc, const char *argv[])
{
	/* Create a directory to contain any Ciln caches in. */
	(void)mkdir(CILN_DIR, 0755);

	/* Until I have hash-checking implemented, I'm going to rebuild every
	   time. That just prevents me from needing to `rm .ciln/build` with
	   every change. */
	/* ~~Build the buildscript, if needed. TODO: Check hashes.~~ */
	// if (stat(CILN_OUTPUT, NULL) == -1)
	// {
		fprintf(stderr, "ciln: compiling build.c\n");
		struct ciln_res res = ciln_system((const char *[]){ CILN_CC, ("-o" CILN_OUTPUT), "-lciln", "build.c", NULL});
		if (res.status == ciln_status_err)
		{
			fprintf(stderr, "ciln: error: failed to compile buildscript.\n");
			return 1;
		}
	// }

	/* Execute the output file and pass arguments off to it. */
	return execv(CILN_OUTPUT, (char *const *)argv);
}
