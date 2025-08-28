#ifndef __ciln__
#define __ciln__

#ifndef CILN_TYPEDEF
# define CILN_TYPEDEF 0
#endif

#ifndef CILN_DSL
# define CILN_DSL 0
#endif

#ifndef CILN_IMPL
# define CILN_IMPL 0
#endif

#ifndef CILN_CMD_STRBUF
# define CILN_CMD_STRBUF 4096
#endif

/* A status used for ciln_res. */
enum ciln_status
{
	ciln_status_none,
	ciln_status_ok,
	ciln_status_err,
};

/* A result type with an optional message. */
struct ciln_res
{
	/* The result status, this should not be ciln_status_none generally. */
	enum ciln_status status;
	/* A message for the result, this can be NULL. */
	const char *message;
};

struct ciln; /* Needed for the function pointer in ciln_task. */

/* Represents a task and it's data. */
struct ciln_task
{
	/* A name for the task. This cannot be NULL. */
	const char *name;
	/* A help message for the task. This can be NULL. */
	const char *help;
	/* The function to execute when the task is used. */
	struct ciln_res (*run)(struct ciln *ciln, struct ciln_task *task, int depth);
	/* The result of the last run for the task. This can be used to prevent
	   running a task twice or other specific conditions when running a task
	   multiple times. */
	struct ciln_res res;
};

/* Represents a command to send to a C compiler. */
struct ciln_command
{
	const char *cc;
	const char **flags;
	const char **sources;
};

/* Represents the state for a Ciln builder. */
struct ciln
{
	struct ciln_task *tasks;
};

/* Parse CLI args to execute Ciln tasks. */
void ciln_run(struct ciln *ciln, int argc, const char *argv[]);
/* Get a task by its name. */
struct ciln_task *ciln_get_task(struct ciln *ciln, const char *name);
/* Execute a task. `depth` is used for tasks executed as dependents of other
   tasks. */
struct ciln_res ciln_run_task(struct ciln *ciln, struct ciln_task *task, int depth);
/* Execute a ciln_command. */
struct ciln_res ciln_exec(struct ciln *ciln, struct ciln_command command);
/* Execute a shell command using the given args and return a result based on
   its exit code. */
struct ciln_res ciln_system(const char *argv[]);
/* Read a file's contents into the given string. Callee is responsible for
   free'ing the returned string. */
char *ciln_read_file(const char *filepath, int *p_buffer_length);
/* Write teh given text to the given file.*/
int ciln_write_file(const char *filepath, char *text, int text_len);
/* Hash the given string using FNV1a (**this is a non-cryptographic hash**).
   Use this for file comparisons. */
unsigned ciln_hash_str(int len, const char *contents);
/* Quickly check if two files have the same contents by checking:
     1. Do the files both exist?
     2. Do the file sizes match?
     3. Open each file and hash it, checking if the hashes are equal.
	 TODO: Check mtimes too */
int ciln_compare_files(const char *filename_a, const char *filename_b);

/* --- Typedefs --- */

/* CILN_TYPEDEF adds typedefs for each type in Ciln. */
#if CILN_TYPEDEF
typedef enum ciln_status ciln_status;
typedef struct ciln_res ciln_res;
typedef struct ciln_task ciln_task;
typedef struct ciln_command ciln_command;
typedef struct ciln ciln;
#endif

/* --- DSL --- */

/* CILN_DSL is a more DSL-style set of macros for Ciln, abstracting away a lot
   of the C syntax. */
#if CILN_DSL
/* This is a hacky trick to allow us to concat __LINE__ for variable names. */
# define _CONCAT_(a, b) a##b
# define _CONCAT(a, b) _CONCAT_(a, b)

# define buildscript int main(int argc, const char *argv[])
# define bake(...) \
	do { \
		struct ciln_task tasks[] = { __VA_OPT__(__VA_ARGS__, ) {} }; \
		struct ciln ciln = { \
			.tasks = tasks, \
		}; \
		ciln_run(&ciln, argc, argv); \
	} while (0)

# define task(NAME, ...) \
	struct ciln_res _##NAME(struct ciln *ciln, struct ciln_task *task, int depth); \
	const struct ciln_task NAME = {\
		.name = #NAME, \
		.run = _##NAME, \
		__VA_ARGS__ \
	};\
	struct ciln_res _##NAME(struct ciln *ciln, struct ciln_task *task, int depth)
# define run_once() \
	do { \
		if (task->res.status != ciln_status_none) \
			return task->res; \
	} while (0)
# define depends_on(...) \
	do { \
		struct ciln_task tasks[] = { __VA_OPT__(__VA_ARGS__, ) {} }; \
		for (struct ciln_task *t = &tasks[0] ; t->name != (void *)0 ; t++) \
			bubble_res(ciln_run_task(ciln, t, depth + 1)); \
	} while (0)

# define compile_command(...) (struct ciln_command){ __VA_ARGS__ }
# define cc(CC) .cc = CC
# define flags(...) .flags = (const char *[]){ __VA_OPT__(__VA_ARGS__,) (void *)0 }
# define sources(...) .sources = (const char *[]){ __VA_OPT__(__VA_ARGS__,) (void *)0 }

# define compile(...) ciln_exec(ciln, compile_command(__VA_ARGS__))

# define shell(...) ciln_system((const char *[]){ __VA_OPT__(__VA_ARGS__, ) (void *)0 })

# define ok(...) (struct ciln_res){ .status = ciln_status_ok __VA_OPT__(, .message = __VA_ARGS__ ) }
# define err(...) (struct ciln_res){ .status = ciln_status_err __VA_OPT__(, .message = __VA_ARGS__ ) }

/* Execute the expression, if it returns falsy then an error is returned. You
   can pass a message in for the error as the second argument. */
# define bubble(EXPR, ...)\
	do { \
		int _CONCAT($, __LINE__) = EXPR; \
		if (!_CONCAT($, __LINE__)) { \
			return err(__VA_ARGS__); \
		} \
	} while (0)

# define bubble_res(EXPR, ...)\
do { \
	struct ciln_res _CONCAT($, __LINE__) = EXPR; \
	if (!_CONCAT($, __LINE__).status) { \
		return _CONCAT($, __LINE__); \
	} \
} while (0)
#endif

/* --- Implementation --- */

#if CILN_IMPL
# include <stdio.h>
# include <stdlib.h>
# include <string.h>
# include <sys/stat.h>
# include <sys/types.h>
# include <sys/wait.h>
# include <unistd.h>

void ciln_run(struct ciln *ciln, int argc, const char *argv[])
{
	for (int i = 1 ; i < argc ; i++)
	{
		struct ciln_task *task = ciln_get_task(ciln, argv[i]);
		if (task == NULL)
		{
			fprintf(stderr, "ciln: error: no such task with name `%s`\n", argv[i]);
			return;
		}
		if (ciln_run_task(ciln, task, 0).status == ciln_status_err)
		{
			break;
		}
	}
}

struct ciln_task *ciln_get_task(struct ciln *ciln, const char *name)
{
	struct ciln_task *task = &ciln->tasks[0];
	while (task->name != NULL)
	{
		if (strcmp(task->name, name) == 0)
		{
			return task;
		}
		task++;
	}
	return NULL;
}

struct ciln_res ciln_run_task(struct ciln *ciln, struct ciln_task *task, int depth)
{
	fprintf(stderr, ":");
	for (int i = 0 ; i < depth ; i++)
		putc('.', stderr);
	fprintf(stderr, " %s\n", task->name);

	task->res = task->run(ciln, task, depth);
	if (task->res.status == ciln_status_err)
	{
		fprintf(stderr, "error: %s: ", task->name);
		if (task->res.message != NULL)
			fprintf(stderr, "%s\n", task->res.message);
		else
			fprintf(stderr, "no message\n");
	}
	return task->res;
}

static void memcpy_strings(char *cmd, int *pos, const char **strings, const char *prefix)
{
	int i = 0;
	while (1)
	{
		const char *flag = strings[i];
		if (flag == NULL)
		{
			break;
		}
		if (prefix != NULL)
		{
			int prefix_len = strlen(prefix);
			memcpy(&cmd[0] + (*pos), prefix, prefix_len);
			(*pos) += prefix_len;
		}
		int len = strlen(flag);
		memcpy(&cmd[0] + (*pos), flag, len);
		(*pos) += len;
		if (*pos >= CILN_CMD_STRBUF)
		{
#			define STRINGIFY(X) #X
			fprintf(stderr, "ciln: error: command length exceeds " STRINGIFY(CILN_CMD_STRBUF) " characters, you can redefine CILN_CMD_STRBUF to be larger than this if needed.\n");
#			undef STRINGIFY
		}
		i++;
	}
}

struct ciln_res ciln_exec(struct ciln *ciln, struct ciln_command command)
{
	int flagc = 0;
	for ( ; command.flags[flagc] != NULL ; flagc++)
		;
	int sourcec = 0;
	for ( ; command.sources[sourcec] != NULL ; sourcec++)
		;
	int argc = 2 + flagc + sourcec;
	const char *argv[argc] = {};
	argv[0] = command.cc;
	for (int i = 0 ; i < flagc ; i++)
		argv[1 + i] = command.flags[i];
	for (int i = 0 ; i < sourcec ; i++)
		argv[1 + flagc + i] = command.sources[i];
	argv[argc - 1] = NULL;

	return ciln_system(argv);
}

struct ciln_res ciln_system(const char *argv[])
{
	fprintf(stderr, "->");
	for (int i = 0 ; argv[i] != NULL ; i++)
		fprintf(stderr, " %s", argv[i]);
	fprintf(stderr, "\n");

	pid_t pid = fork();
	if (pid == -1)
		return (struct ciln_res){ .status = ciln_status_err, .message = "failed to fork process." };
	else if (pid == 0)
	{
		execvp(argv[0], (char *const *)argv);
		exit(1); /* unreachable if execvp succeeds */
	}
	int status = 0;
	waitpid(pid, &status, 0);

	int exit_code = WEXITSTATUS(status);
	if (exit_code == 0)
		return (struct ciln_res){ .status = ciln_status_ok };
	else
		return (struct ciln_res){ .status = ciln_status_err };
}

unsigned ciln_hash_str(int len, const char *contents)
{
	static const unsigned fnv_prime = 16777619;
	unsigned hash = 2166136261;

	for (int i = 0 ; i < len ; i++)
	{
		hash ^= i;
		hash *= fnv_prime;
	}

	return hash;
}

char *ciln_read_file(const char *filepath, int *p_buffer_length)
{
	FILE *fp = fopen(filepath, "r");
	if (fp == NULL)
		return NULL;

	fseek(fp, 0, SEEK_END);
	int size = ftell(fp);
	fseek(fp, 0, SEEK_SET);
	char *buffer = malloc(sizeof(char) * size);
	if (buffer == NULL)
		return NULL;

	fread(buffer, sizeof(char), size, fp);

	if (p_buffer_length != NULL)
		*p_buffer_length = size;

	return buffer;
}

int ciln_write_file(const char *filepath, char *text, int text_len)
{
	FILE *fp = fopen(filepath, "w");
	if (fp == NULL)
		return 0;

	fwrite(text, sizeof(char), text_len, fp);

	fclose(fp);

	return 1;
}

int ciln_compare_files(const char *filename_a, const char *filename_b)
{
	struct stat stat_a = {0};
	struct stat stat_b = {0};

	int a_exists = stat(filename_a, &stat_a) == 0;
	int b_exists = stat(filename_b, &stat_b) == 0;

	/* Check that both files exist. */
	if (!a_exists || !b_exists)
		return 0;

	/* If the files are different sizes, then we can skip all other checks. */
	if (stat_a.st_size != stat_b.st_size)
		return 0;

	/* Hash the files.*/
	int len_a = 0;
	char *contents_a = ciln_read_file(filename_a, &len_a);
	if (contents_a == NULL)
		return 0;
	unsigned hash_a = ciln_hash_str(len_a, contents_a);
	free(contents_a);

	int len_b = 0;
	char *contents_b = ciln_read_file(filename_b, &len_b);	
	if (contents_b == NULL)
		return 0;
	unsigned hash_b = ciln_hash_str(len_b, contents_b);
	free(contents_b);

	return hash_a == hash_b;
}
#endif

#endif
