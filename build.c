#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

#define CILN_TYPEDEF 1
#define CILN_DSL 1
#include "ciln.h"

const char *cc = "cc";

task(clean, .help = "Delete compiled files")
{
	run_once();
	remove("libciln.so");
	remove("ciln");
	return ok();
}

task(libciln, .help = "Build libciln.so")
{
	run_once();
	return compile(
		cc(cc),
		flags("-fPIC", "-shared", "-olibciln.so"),
		sources("ciln.c"),
	);
}

task(build, .help = "Build ciln")
{
	run_once();
	return compile(
		cc(cc),
		flags("-ociln", "-lciln"),
		sources("main.c"),
	);
}

task(symlink_, .name = "symlink", .help = "Make symlinks for Ciln's executables and libraries. Requires sudo")
{
	run_once();

	char path[BUFSIZ] = {};
	if (getcwd(path, BUFSIZ) == NULL)
		return err("failed to get current working directory.");

	char ciln_path[BUFSIZ];
	strcat(ciln_path, path);
	strcat(ciln_path, "/ciln");
	if (stat(ciln_path, NULL) == -1)
		bubble(symlink(ciln_path, "/usr/local/bin/ciln") == 0, "failed to symlink ciln to /usr/local/bin/ciln");

	char libciln_path[BUFSIZ];
	strcat(libciln_path, path);
	strcat(libciln_path, "/libciln.so");
	if (stat(libciln_path, NULL) == -1)
		bubble(symlink(libciln_path, "/usr/lib/libciln.so") == 0, "failed to symlink libciln.so to /usr/lib/libciln.so");

	char ciln_header_path[BUFSIZ];
	strcat(ciln_header_path, path);
	strcat(ciln_header_path, "/ciln.h");
	if (stat(ciln_header_path, NULL) == -1)
		bubble(symlink(ciln_header_path, "/usr/local/include/ciln.h") == 0, "failed to symlink ciln.h to /usr/local/include/ciln.h");

	printf("note: you will need to run `sudo ldconfig` to reload the linker's library cache.\n");

	return ok();
}

task(uninstall)
{
	run_once();

	if (stat("/usr/local/bin/ciln", NULL) == -1)
		bubble(remove("/usr/local/bin/ciln") == 0, "failed to remove/local/bin/ciln");

	if (stat("/usr/lib/libciln.so", NULL) == -1)
		bubble(remove("/usr/lib/libciln.so") == 0, "failed to remove /usr/lib/libciln.so");

	if (stat("/usr/local/include/ciln.h", NULL) == -1)
		bubble(remove("/usr/local/include/ciln.h") == 0, "failed to remove /usr/local/include/ciln.h");

	return ok();
}

task(hello, .help = "Hello, World!")
{
	fprintf(stderr, "Hello, World!\n");
	return ok();
}

buildscript
{
	bake(clean, libciln, build, symlink_, uninstall, hello);
}
