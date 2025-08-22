#ifndef __ciln_c__
#define __ciln_c__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include "ciln.h"

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
	fprintf(stderr, "-> %s", command.cc);
	for (int i = 0 ; command.flags[i] != NULL ; i++)
		fprintf(stderr, " %s", command.flags[i]);
	for (int i = 0 ; command.sources[i] != NULL ; i++)
		fprintf(stderr, " %s", command.sources[i]);
	fprintf(stderr, "\n");

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

#endif
