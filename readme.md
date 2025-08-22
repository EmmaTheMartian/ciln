# Ciln

> Pronounced like "kiln"

Ciln is a build system written in and for C.

## Installation

```sh
# Clone the repo
git clone https://github.com/emmathemartian/ciln
cd ciln

# Build the Ciln executable and library, which compile and execute your build.c
cc -ociln ./main.c
cc -olibciln.so -fPIC -shared ./ciln.c

# For installation, you have two options. You can either symlink the library and
# executable to /usr/local/lib/ and /usr/local/bin/ respectively, or you can
# directly copy them. To symlink, use:
sudo ./ciln symlink
# and to copy:
sudo ./ciln install
# Symlinking allows you to update Ciln by `git pull`ing and rebuilding, i.e, you
# can maintain an up-to-date and from-source distribution of Ciln easily.
```

## Usage

Ciln's CLI is fundamentally as simple as possible. I do not believe there needs
to be a lot of options nor subcommands, and so there aren't.

```sh
# Execute the `build` task
ciln build

# Execute `clean`, then `build`
ciln clean build

# Forcefully recompile build.c
rm .ciln/build
ciln

# In the event that you need to build a Ciln build.c without the `ciln` tool,
# you can simply:
cc -o.ciln/build -lciln build.c
```

### With the DSL

Ciln provides a set of macros designed to abstract away a lot of the C-ness from
your build.c, making it feel more like a build script than C.

Generally this is what I recommend using, as it removes a _lot_ of boilerplate
without straying _too_ far from C (for the most part, at least).

```c
/* Sample build.c */
#include <ciln.h>

/* Create a task called `build` which compiles main.c */
task(build, .help = "Build the program")
{
	run_once(); /* Prevent this task from running twice in one `ciln` call. */
	return compile(
		cc("cc"),
		flags("-omain", "-g"),
		sources("main.c"),
	);
}

task(test)
{
	run_once();
	/* `bubble` and `bubble_res` are macros which evaluate the given
	   expression, and if it's falsy (or ciln_status_err for bubble_res)
	   then the task will return an error, you can also optionally provide a
	   second argument to be the message of the error. */
	bubble_res(compile(
		cc("cc"),
		flags("-omain", "-g"),
		sources("main.c"),
	), "failed to compile main.c");
	bubble(system("./main"));
	/* If we didn't have an error, we'll return ok. */
	return ok();
}

/* `buildscript` is a shorthand for `int main(int argc, const char *argv[])`,
   feel free to completely forego it if you prefer. */
buildscript
{
	/* `bake` creates and runs a build context, the provided arguments is a
	   list of tasks to add. */
	bake(build, test);
}
```

### Without the DSL

```c
/* Sample build.c */
#include <ciln.h>

struct ciln_res _build(struct ciln *ciln, struct ciln_task *task, int depth)
{
	if (task->res.status != ciln_status_none)
		return task->res; /* Prevent this task from running twice in one `ciln` call. */

	return ciln_exec(ciln, (struct ciln_command){
		.cc = "cc",
		.flags = { "-omain", "-g", NULL },
		.sources = { "main.c", NULL },
	})
}

const ciln_task build = (struct ciln_task){
	.name = "build",
	.help = "Build the program",
	.run = _build,
};

int main(int argc, const char *argv[])
{
	struct ciln_task tasks[] = {
		build,
		{} /* Sentinel */
	};
	struct ciln ciln = { .tasks = tasks };
	ciln_run(&ciln, argc, argv);
}
```
