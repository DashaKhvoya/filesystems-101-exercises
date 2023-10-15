#include "solution.h"
#include <dirent.h>
#include <stdlib.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>

#define MAX_FILE_SIZE 4096 * 100

static const char *proc_root_dir = "/proc";

struct solution
{
	char *proc_dir;

	char *exe_path;

	char *cmdline_path;
	char cmdline_buf[MAX_FILE_SIZE];
	char **cmdline_args;

	char *environ_path;
	char environ_buf[MAX_FILE_SIZE];
	char **environ_args;

	int pid;
	int error;
};

static void get_solution(struct solution *sol, const char *dir, const char *name);
static void free_solution(struct solution sol);

static int get_exe_path(struct solution *sol);
static int get_cmdline_args(struct solution *sol);
static int get_environ_args(struct solution *sol);

static char *get_full_path(const char *dir, const char *file);
static char **get_array_from_string(char *str, long size);

void ps(void)
{
	DIR *root_dir = opendir(proc_root_dir);
	if (!root_dir)
	{
		report_error(proc_root_dir, errno);
		return;
	}

	struct solution sol = {
		.proc_dir = NULL,

		.exe_path = NULL,

		.cmdline_path = NULL,
		.cmdline_buf = "",
		.cmdline_args = NULL,

		.environ_path = NULL,
		.environ_buf = "",
		.environ_args = NULL,

		.pid = 0,
		.error = 0};

	struct dirent *entry;
	while ((entry = readdir(root_dir)) != NULL)
	{
		int is_pid_name = 1;
		for (int i = 0; entry->d_name[i]; i++)
		{
			if (entry->d_name[i] < '0' || entry->d_name[i] > '9')
			{
				is_pid_name = 0;
				break;
			}
		}
		if (!is_pid_name)
		{
			continue;
		}

		get_solution(&sol, proc_root_dir, entry->d_name);
		if (!sol.error) {
			report_process(sol.pid, sol.exe_path, sol.cmdline_args, sol.environ_args);			
		}

		free_solution(sol);
	}

	closedir(root_dir);
}

void get_solution(struct solution *sol, const char *dir, const char *name)
{	
	sol->proc_dir = NULL;

	sol->exe_path = NULL;

	memset(sol->cmdline_buf, 0, MAX_FILE_SIZE);
	sol->cmdline_path = NULL;
	sol->cmdline_args = NULL;

	memset(sol->environ_buf, 0, MAX_FILE_SIZE);
	sol->environ_path = NULL;
	sol->environ_args = NULL;

	sol->pid = 0;
	sol->error = 0;

	sol->pid = atoi(name);
	if (sol->pid == 0) {
		sol->error = 1;
		return;
	}

	sol->proc_dir = get_full_path(dir, name);
	if (!sol->proc_dir)
	{
		sol->error = 1;
		return;
	}

	int err = get_exe_path(sol);
	if (err)
	{
		sol->error = err;
		return;
	}

	err = get_cmdline_args(sol);
	if (err)
	{
		sol->error = err;
		return;
	}

	err = get_environ_args(sol);
	if (err)
	{
		sol->error = err;
		return;
	}

	return;
}

void free_solution(struct solution sol)
{
	free(sol.proc_dir);

	free(sol.exe_path);

	free(sol.cmdline_path);
	free(sol.cmdline_args);

	free(sol.environ_path);
	free(sol.environ_args);
}

int get_exe_path(struct solution *sol)
{
	char *exe_path = get_full_path(sol->proc_dir, "exe");
	if (!exe_path)
	{
		return errno;
	}

	sol->exe_path = realpath(exe_path, NULL);
	if (!sol->exe_path)
	{
		report_error(exe_path, errno);
		free(exe_path);
		return errno;
	}

	free(exe_path);
	return 0;
}

int get_cmdline_args(struct solution *sol)
{
	sol->cmdline_path = get_full_path(sol->proc_dir, "cmdline");
	if (!sol->cmdline_path)
	{
		return errno;
	}

	FILE *file = fopen(sol->cmdline_path, "rb");
	if (!file)
	{
		report_error(sol->cmdline_path, errno);
		return errno;
	}

	long file_size = fread(sol->cmdline_buf, sizeof(char), MAX_FILE_SIZE, file);
	if (ferror(file)) {
		return ferror(file);
	}

	fclose(file);

	sol->cmdline_args = get_array_from_string(sol->cmdline_buf, file_size);
	if (!sol->cmdline_args)
	{
		return errno;
	}

	return 0;
}

int get_environ_args(struct solution *sol)
{
	sol->environ_path = get_full_path(sol->proc_dir, "environ");
	if (!sol->environ_path)
	{
		return errno;
	}

	FILE *file = fopen(sol->environ_path, "rb");
	if (!file)
	{
		report_error(sol->environ_path, errno);
		return errno;
	}

	long file_size = fread(sol->environ_buf, sizeof(char), MAX_FILE_SIZE, file);
	if (ferror(file)) {
		return ferror(file);
	}

	fclose(file);

	sol->environ_args = get_array_from_string(sol->environ_buf, file_size);
	if (!sol->environ_args)
	{
		return errno;
	}

	return 0;
}

char *get_full_path(const char *dir, const char *file)
{
	// Consider '/' and NULL at the end
	size_t full_size = strlen(dir) + strlen(file) + 2;
	char *buf = malloc(full_size);
	sprintf(buf, "%s/%s", dir, file);

	return buf;
}

char **get_array_from_string(char *str, long size)
{
	int arr_size = 0;
	for (int i = 0; i < size; i++)
	{
		if (str[i] == '\0')
		{
			arr_size++;
		}
	}

	char **arr = malloc(arr_size + 1);

	if (arr_size == 0 || size == 0)
	{
		return arr;
	}

	int curr_el = 0;
	char *curr_str = str;
	for (int i = 0; i < size; i++)
	{
		if (str[i] == '\0')
		{
			arr[curr_el] = curr_str;
			curr_el++;
			curr_str = str + i + 1;
		}
	}

	return arr;
}