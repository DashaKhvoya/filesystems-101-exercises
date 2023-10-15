#include "solution.h"
#include <stdlib.h>
#include <stdio.h>
#include <limits.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <assert.h>

void abspath(const char *input)
{
	if (input == NULL || strlen(input) < 1)
	{
		return;
	}

	char *path = strdup(input);
	char *copy = path;
	char result[PATH_MAX] = "";
	char link[PATH_MAX] = "";

	char *token, *saveptr;
	while ((token = strtok_r(path, "/", &saveptr)) != NULL)
	{
		path = NULL;

		if (!strcmp(token, "."))
		{
			continue;
		}
		if (!strcmp(token, ".."))
		{
			if (strlen(result) > 1)
			{
				char *last = strrchr(result, '/');
				*last = '\0';
			}
			continue;
		}

		char tmp_path[2 * PATH_MAX] = "";
		snprintf(tmp_path, 2 * PATH_MAX, "%s/%s", result, token);

		struct stat path_stat;
		lstat(tmp_path, &path_stat);
		if (errno)
		{
			report_error(result, token, errno);
			free(copy);
			return;
		}
		if (S_ISLNK(path_stat.st_mode))
		{
			// loop?
			ssize_t link_len = readlink(tmp_path, link, PATH_MAX - 1);
			if (errno)
			{
				report_error(result, token, errno);
				free(copy);
				return;
			}

			link[link_len] = '\0';
			if (link[0] == '/')
			{
				char *last = strrchr(link, '/');
				*last = '\0';
				
				strncpy(tmp_path, link, PATH_MAX);
			}
			else if (!strcmp(link, "."))
			{
				strncpy(tmp_path, result, PATH_MAX);
			}
			else if (!strcmp(link, ".."))
			{
				if (strlen(result) > 0) 
				{
					char *last = strrchr(result, '/');
					*last = '\0';
				}
				strncpy(tmp_path, result, PATH_MAX);
			}
			else (link_len > 0)
			{
 				char *last = strrchr(link, '/');
				if (last != NULL)
				{
					*last = '\0';
				}
				snprintf(tmp_path, 2 * PATH_MAX, "%s/%s", result, link);
			}
		}

		strncpy(result, tmp_path, PATH_MAX);
	}

	struct stat path_stat;
	stat(result, &path_stat);
	if (strlen(result) == 0 || S_ISDIR(path_stat.st_mode))
	{
		result[strlen(result) + 1] = '\0';
		result[strlen(result)] = '/';
	}

	free(copy);
	report_path(result);
}