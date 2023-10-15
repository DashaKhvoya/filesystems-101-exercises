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

#include <sys/param.h>
#include <sys/stat.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

void abspath(const char *input)
{
	if (input == NULL || strlen(input) < 1)
	{
		return;
	}

	// Result
	char result[PATH_MAX] = "";
	// Link path
	char link[PATH_MAX] = "";
	char current[PATH_MAX] = "";

	char token[PATH_MAX] = "";
	char *next_token_ptr = NULL;

	if (input[1] == '\0')
	{
		report_path(input);
		return;
	}

	// Setup current
	if (input[0] == '/')
	{
		strncpy(current, input + 1, PATH_MAX);
	}
	else
	{
		strncpy(current, input, PATH_MAX);
	}

	while (strlen(current))
	{
		next_token_ptr = strchr(current, '/');
		if (next_token_ptr != NULL)
		{
			*next_token_ptr = '\0';
			strncpy(token, current, PATH_MAX);
			strncpy(current, next_token_ptr + 1, PATH_MAX);
		}
		else
		{
			strncpy(token, current, PATH_MAX);
			current[0] = '\0';
		}

		if (token[0] == '\0')
		{
			continue;
		}
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

		char parent[PATH_MAX] = "";
		char tmp_path[2 * PATH_MAX] = "";
		snprintf(tmp_path, 2 * PATH_MAX, "%s/%s", result, token);
		// printf("tmp path: <%s>\n", tmp_path);

		struct stat path_stat;
		lstat(tmp_path, &path_stat);
		if (errno)
		{
			if (strlen(result) == 0)
			{
				report_error("/", token, errno);
			}
			else
			{
				report_error(result, token, errno);
			}
			return;
		}

		strncpy(parent, result, PATH_MAX);
		strncpy(result, tmp_path, PATH_MAX);
		// printf("result now <%s>\n", result);

		if (S_ISLNK(path_stat.st_mode))
		{
			ssize_t link_len = readlink(result, link, PATH_MAX - 1);
			if (errno)
			{
				if (strlen(parent) == 0)
				{
					report_error("/", token, errno);
				}
				else
				{
					report_error(parent, token, errno);
				}
				return;
			}
			link[link_len] = '\0';
			// printf("readed line <%s>\n", link);

			if (link[0] == '/')
			{
				result[0] = '\0';
			}
			else if (strlen(result) > 1)
			{
				char *last = strrchr(result, '/');
				*last = '\0';
			}

			// printf("current in link: <%s>\n", current);
			if (strlen(current) > 0)
			{
				if (link[link_len - 1] != '/')
				{
					link[link_len] = '/';
					link[link_len + 1] = '\0';
				}
				strlcat(link, current, PATH_MAX);
			}

			strncpy(current, link, PATH_MAX);
			// printf("current at the end <%s>, result <%s>\n", current, result);
		}
	}

	struct stat path_stat;
	stat(result, &path_stat);
	if (strlen(result) == 0 || S_ISDIR(path_stat.st_mode))
	{
		result[strlen(result) + 1] = '\0';
		result[strlen(result)] = '/';
	}

	report_path(result);
}