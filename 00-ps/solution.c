#include "solution.h"
#include <dirent.h>
#include <stdlib.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>

const char *proc_root_dir = "/proc";

struct solution
{
	char *proc_dir;

	char *exe_path;

	char *cmdline_path;
	char *cmdline_buf;
	char **cmdline_args;

	char *environ_path;
	char *environ_buf;
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
		if (entry->d_type != DT_DIR)
		{
			continue;
		}
		int is_pid_name = 1;
		for (int i = 0; i < entry->d_namlen; i++)
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
		if (!sol.error)
		{
			report_process(sol.pid, sol.exe_path, sol.cmdline_args, sol.environ_args);
		}

		free_solution(sol);
	}
}

struct solution get_solution(const char *dir, const char *name)
{
	struct solution sol;
	sol.pid = atoi(name);

	sol.proc_dir = get_full_path(dir, name);
	if (errno)
	{
		sol.error = errno;
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

	sol.error = 0;
	return sol;
}

void free_solution(struct solution sol)
{
	free(sol.proc_dir);

	free(sol.exe_path);

	free(sol.cmdline_path);
	free(sol.cmdline_buf);
	free(sol.cmdline_args);

	free(sol.environ_path);
	free(sol.environ_buf);
	free(sol.environ_args);
}

int get_exe_path(struct solution *sol)
{
	char *exe_path = get_full_path(sol->proc_dir, "exe");
	if (errno)
	{
		return errno;
	}

	char *real_path = realpath(exe_path, NULL);
	if (errno)
	{
		report_error(exe_path, errno);
		free(exe_path);
		return errno;
	}

	sol->exe_path = real_path;
	return 0;
}

int get_cmdline_args(struct solution *sol)
{
	sol->cmdline_path = get_full_path(sol->proc_dir, "cmdline");
	if (errno)
	{
		return errno;
	}

	FILE *file = fopen(sol->cmdline_path, "rb");
	if (errno)
	{
		report_error(sol->cmdline_path, errno);
		return errno;
	}

	int err = fseek(file, 0, SEEK_END);
	if (err)
	{
		return err;
	}

	long file_size = ftell(file);
	if (errno)
	{
		return errno;
	}

	err = fseek(file, 0, SEEK_SET);
	if (err)
	{
		return err;
	}

	sol->cmdline_buf = (char *)calloc(file_size, sizeof(char));
	if (errno)
	{
		return errno;
	}

	fread(sol->cmdline_buf, file_size, sizeof(char), file);
	if (errno)
	{
		return errno;
	}

	fclose(file);

	sol->cmdline_args = get_array_from_string(sol->cmdline_buf, file_size);
	if (errno)
	{
		return errno;
	}

	return 0;
}

int get_environ_args(struct solution *sol)
{
	sol->environ_path = get_full_path(sol->proc_dir, "environ");
	if (errno)
	{
		return errno;
	}

	FILE *file = fopen(sol->environ_path, "rb");
	if (errno)
	{
		report_error(sol->environ_path, errno);
		return errno;
	}

	int err = fseek(file, 0, SEEK_END);
	if (err)
	{
		return err;
	}

	long file_size = ftell(file);
	if (errno)
	{
		return errno;
	}

	err = fseek(file, 0, SEEK_SET);
	if (err)
	{
		return err;
	}

	sol->environ_buf = (char *)calloc(file_size, sizeof(char));
	if (errno)
	{
		return errno;
	}

	fread(sol->environ_buf, file_size, sizeof(char), file);
	if (errno)
	{
		return errno;
	}

	fclose(file);

	sol->environ_args = get_array_from_string(sol->environ_buf, file_size);
	if (errno)
	{
		return errno;
	}

	return 0;
}

char *get_full_path(const char *dir, const char *file)
{
	// Consider '/' and NULL at the end
	size_t full_size = strlen(dir) + strlen(file) + 2;
	char *buf = (char *)calloc(full_size, sizeof(char));
	if (errno)
	{
		return NULL;
	}
	sprintf(buf, "%s/%s", dir, file);
	if (errno)
	{
		free(buf);
		return NULL;
	}

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
	if (errno)
	{
		return NULL;
	}
	arr[arr_size] = NULL;
	if (arr_size == 0)
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