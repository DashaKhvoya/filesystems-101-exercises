#include "solution.h"
#include <stdlib.h>
#include <stdio.h>
#include <limits.h>
#include <fcntl.h>
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

	// Result
	char result[PATH_MAX] = "";
	int result_fd = open("/", O_DIRECTORY | O_RDONLY);
	if (result_fd == -1) {
		report_error("/", "", errno);
		return;
	}
	// Link pat
	char link[PATH_MAX] = "";
	char current[PATH_MAX] = "";

	char token[PATH_MAX] = "";
	char *next_token_ptr = NULL;

	if (input[1] == '\0')
	{
		report_path(input);
		close(result_fd);
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
			memmove(current, next_token_ptr + 1, strlen(next_token_ptr+1) + 1);
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
		
		strcat(result, "/");
		snprintf(tmp_path, 2 * PATH_MAX, "%s%s", result, token);
		// printf("tmp path: <%s>\n", tmp_path);
		//int tmp_fd = openat(result_fd, token, O_RDONLY | O_DIRECTORY);
		//if (tmp_fd == -1) {
		//	if (strlen(result) > 1) 
		//	{
		//		char *last = strrchr(result, '/');
		//		*last = '\0';
		//	}
		//	report_error(result, token, errno);
		//	return;
		//}
		struct stat path_stat;
		if (fstatat(result_fd, token, &path_stat, AT_SYMLINK_NOFOLLOW) < 0) {
    		if (strlen(result) > 1)
			{
				char *last = strrchr(result, '/');
				*last = '\0';
			}
			report_error(result, token, errno);
			close(result_fd);
			return;
		}

		strncpy(parent, result, PATH_MAX);
		strncpy(result, tmp_path, PATH_MAX);
		// printf("result now <%s>\n", result);

		if (S_ISLNK(path_stat.st_mode))
		{
			ssize_t link_len = readlinkat(result_fd, token, link, PATH_MAX - 1);
			if (errno)
			{
				report_error(parent, token, errno);
				close(result_fd);
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
				strcat(link, current);
			}

			strncpy(current, link, PATH_MAX);
			// printf("current at the end <%s>, result <%s>\n", current, result);
		}
		
		int new_result_fd = openat(result_fd, token, O_DIRECTORY | O_RDONLY);
		close(result_fd);
		if (result_fd == -1) {
			report_error(parent, token, errno);
			return;
		}
		
		result_fd = new_result_fd;
	}

	struct stat path_stat;
	stat(result, &path_stat);
	if (strlen(result) == 0 || S_ISDIR(path_stat.st_mode))
	{
		result[strlen(result) + 1] = '\0';
		result[strlen(result)] = '/';
	}

	close(result_fd);
	report_path(result);
}