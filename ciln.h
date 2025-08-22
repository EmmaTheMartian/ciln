#ifndef __ciln_h__
#define __ciln_h__

#ifndef CILN_TYPEDEF
# define CILN_TYPEDEF 0
#endif

#ifndef CILN_DSL
# define CILN_DSL 0
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
/* Execute a shell command using the given args and return a result based on its
   exit code. */
struct ciln_res ciln_system(const char *argv[]);

/* CILN_TYPEDEF adds typedefs for each type in Ciln. */
#if CILN_TYPEDEF
typedef enum ciln_status ciln_status;
typedef struct ciln_res ciln_res;
typedef struct ciln_task ciln_task;
typedef struct ciln_command ciln_command;
typedef struct ciln ciln;
#endif

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

#endif
