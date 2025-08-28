#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#define CILN_IMPL 1
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

int build_buildc(void)
{
	fprintf(stderr, "ciln: compiling build.c\n");
	struct ciln_res res = ciln_system((const char *[]){ CILN_CC, ("-o" CILN_OUTPUT), "build.c", NULL});
	if (res.status == ciln_status_err)
	{
		fprintf(stderr, "ciln: error: failed to compile buildscript.\n");
		return 0;
	}

	/* Cache the source of the buildscript in .ciln for later comparisons. */
	int len = 0;
	char *text = ciln_read_file("build.c", &len);
	ciln_write_file(CILN_DIR "/build.c", text, len);
	free(text);

	return 1;
}

int main(int argc, const char *argv[])
{
	/* Create a directory to contain any Ciln caches in. */
	(void)mkdir(CILN_DIR, 0755);

	struct stat buildc_stat = {0};
	struct stat output_stat = {0};

	int buildc_exists = stat("build.c", &buildc_stat) == 0;
	int output_exists = stat(CILN_OUTPUT, &output_stat) == 0;

	if (!buildc_exists)
	{
		fprintf(stderr, "ciln: error: no build.c available.\n");
		return 1;
	}

	/* Build build.c when either the output .ciln/build does not exist OR when
	   the timestamps of the two are different. */
	if (!output_exists || !ciln_compare_files("build.c", ".ciln/build.c"))
	{
		build_buildc();
	}

	/* Execute the output file and pass arguments off to it. */
	return execv(CILN_OUTPUT, (char *const *)argv);
}
