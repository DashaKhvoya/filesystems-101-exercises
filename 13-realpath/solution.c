#include "solution.h"
#include <stdlib.h>
#include <stdio.h>
#include <limits.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>

void abspath(const char *input)
{
	char *path = strdup(input);

	if (path == NULL || strlen(path) < 1)
	{
		return;
	}

	char result[PATH_MAX] = "";

	char *token, *saveptr;
	while ((token = strtok_r(path, "/", &saveptr)) != NULL)
	{
		path = NULL;

		char tmp[PATH_MAX] = "";
		char full_path[PATH_MAX] = "";
		snprintf(tmp, PATH_MAX, "%s/%s", result, token);
		realpath(tmp, full_path);
		if (errno)
		{
			report_error(result, token, errno);
			free(path);
			return;
		}

		strncpy(result, full_path, PATH_MAX);
	}

	struct stat path_stat;
	stat(result, &path_stat);
	if (S_ISDIR(path_stat.st_mode))
	{
		result[strlen(result)] = '/';
	}

	free(path);
	report_path(result);
}