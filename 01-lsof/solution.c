#include "solution.h"
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <dirent.h>
#include <unistd.h>
#include <limits.h>

const char *proc_root_dir = "/proc";

static int is_numeric(const char *name)
{
	int is_num = 1;
	for (int i = 0; name[i]; i++)
	{
		if (name[i] < '0' || name[i] > '9')
		{
			is_num = 0;
			break;
		}
	}
	return is_num;
}

void lsof(void)
{
	DIR *root_dir = opendir(proc_root_dir);
	if (!root_dir)
	{
		report_error(proc_root_dir, errno);
		return;
	}

	char fd_dir_path[PATH_MAX] = "";
	char fd_link_path[2 * PATH_MAX] = "";
	char result[PATH_MAX] = "";

	struct dirent *entry;
	while ((entry = readdir(root_dir)) != NULL)
	{
		if (!is_numeric(entry->d_name))
		{
			continue;
		}

		snprintf(fd_dir_path, PATH_MAX, "%s/%s/fd", proc_root_dir, entry->d_name);

		DIR *proc_fd_dir = opendir(fd_dir_path);
		if (!proc_fd_dir)
		{
			report_error(fd_dir_path, errno);
			continue;
		}

		struct dirent *sub_entry;
		while ((sub_entry = readdir(proc_fd_dir)) != NULL)
		{
			if (!is_numeric(sub_entry->d_name))
			{
				continue;
			}

			snprintf(fd_link_path, 2 * PATH_MAX, "%s/%s", fd_dir_path, sub_entry->d_name);

			ssize_t link_len = readlink(fd_link_path, result, PATH_MAX - 1);
			if (link_len == -1)
			{
				report_error(fd_link_path, errno);
				continue;
			}

			result[link_len] = '\0';
			report_file(result);
		}
		closedir(proc_fd_dir);
	}

	closedir(root_dir);
}
