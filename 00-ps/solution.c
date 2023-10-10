#include "solution.h"
#include <dirent.h>
#include <stdlib.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>

#define MAX_FILE_SIZE 10000

const char *proc_root_dir = "/proc";

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

struct solution get_solution(const char *dir, const char *name);
void free_solution(struct solution sol);

int get_exe_path(struct solution *sol);
int get_cmdline_args(struct solution *sol);
int get_environ_args(struct solution *sol);

char *get_full_path(const char *dir, const char *file);
char **get_array_from_string(char *str, long size);

void ps(void)
{
	DIR *root_dir = opendir(proc_root_dir);
	if (!root_dir)
	{
		report_error(proc_root_dir, errno);
		return;
	}

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

		struct solution sol = get_solution(proc_root_dir, entry->d_name);
		if (!sol.error) {
			printf("  exe = '%s'\n", sol.exe_path);

			printf("  argv = [");
			for (char **x = sol.cmdline_args; *x != NULL; ++x)
				printf("'%s', ", *x);
			printf("]\n");

			printf("  envp = [");
			for (char **x = sol.environ_args; *x != NULL; ++x)
				printf("'%s', ", *x);
			printf("]\n"); 
			report_process(sol.pid, sol.exe_path, sol.cmdline_args, sol.environ_args);			
		}

		free_solution(sol);
	}

	closedir(root_dir);
}

struct solution get_solution(const char *dir, const char *name)
{
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
	sol.pid = atoi(name);
	if (sol.pid == 0) {
		sol.error = 1;
		return sol;
	}

	sol.proc_dir = get_full_path(dir, name);
	if (!sol.proc_dir)
	{
		sol.error = 1;
		return sol;
	}

	int err = get_exe_path(&sol);
	if (err)
	{
		sol.error = err;
		return sol;
	}

	err = get_cmdline_args(&sol);
	if (err)
	{
		sol.error = err;
		return sol;
	}

	err = get_environ_args(&sol);
	if (err)
	{
		sol.error = err;
		return sol;
	}

	return sol;
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
		return 1;
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
		return 1;
	}

	FILE *file = fopen(sol->cmdline_path, "r");
	if (!file)
	{
		report_error(sol->cmdline_path, errno);
		return errno;
	}

	long file_size = fread(sol->cmdline_buf, sizeof(char), MAX_FILE_SIZE, file);
	if (ferror(file)) {
		return 1;
	}
	if (file_size > 0 && sol->cmdline_buf[file_size-1] != 0) {
		file_size++;
	}
	sol->cmdline_buf[file_size-1] = 0;

	fclose(file);

	file_size = 0;
	sol->cmdline_args = get_array_from_string(sol->cmdline_buf, file_size);
	if (!sol->cmdline_args)
	{
		return 1;
	}

	return 0;
}

int get_environ_args(struct solution *sol)
{
	sol->environ_path = get_full_path(sol->proc_dir, "environ");
	if (!sol->environ_path)
	{
		return 1;
	}

	FILE *file = fopen(sol->environ_path, "r");
	if (!file)
	{
		report_error(sol->environ_path, errno);
		return errno;
	}

	long file_size = fread(sol->environ_buf, sizeof(char), MAX_FILE_SIZE, file);
	if (ferror(file)) {
		return 1;
	}
	if (file_size > 0 && sol->environ_buf[file_size-1] != 0) {
		file_size++;
	}
	sol->environ_buf[file_size-1] = 0;

	fclose(file);

	sol->environ_args = get_array_from_string(sol->environ_buf, file_size);
	if (!sol->environ_args)
	{
		return 1;
	}

	return 0;
}

char *get_full_path(const char *dir, const char *file)
{
	// Consider '/' and NULL at the end
	size_t full_size = strlen(dir) + strlen(file) + 2;
	char *buf = (char *)calloc(full_size, sizeof(char));
	if (!buf)
	{
		return NULL;
	}
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

	char **arr = (char **)calloc(arr_size + 1, sizeof(char *));
	if (!arr)
	{
		return NULL;
	}
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

	if (curr_el != arr_size) {
		*(char*)0 = 's';
	}

	for (char** ptr = arr; *ptr != NULL; ptr++) {
		if (*ptr - arr > MAX_FILE_SIZE) {
			*(char*)0 = 's';
		}
	}

	return arr;
}