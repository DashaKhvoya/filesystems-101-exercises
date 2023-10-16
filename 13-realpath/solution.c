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

	char tmp[2*PATH_MAX] = "";
	// Result
	char result[PATH_MAX] = "";
	int result_fd = open("/", O_RDONLY | O_DIRECTORY);
	if (result_fd == -1)
	{
		close(result_fd);
		return;
	}
	// Link path
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
			int new_result_fd = openat(result_fd, "..", O_RDONLY | O_DIRECTORY);
			if (new_result_fd == -1) 
			{
				report_error(result, "..", errno);
				close(result_fd);
				return;
			}
			close(result_fd);
			result_fd = new_result_fd;

			continue;
		}

		snprintf(tmp, 2*PATH_MAX, "%s/", result);
		strncpy(result, tmp, PATH_MAX);
		int new_result_fd = openat(result_fd, token, O_RDONLY | O_NOFOLLOW);
		if (new_result_fd == -1) 
		{
			//printf("result <%s>, token <%s>\n", result, token);
			//printf("errno = %d\n", errno);
			if (errno == ELOOP) // Ссылка
			{
				ssize_t link_len = readlinkat(result_fd, token, link, PATH_MAX - 1);
				if (link_len == -1)
				{
					report_error(result, token, errno);	
					return;
				}

				link[link_len] = '\0';
				if (link[0] == '/')
				{
					close(result_fd);
					result_fd = open("/", O_DIRECTORY | O_RDONLY);
					if (result_fd == -1) {
						return;
					}

					result[0] = '\0';
				}
				else if (strlen(result) > 1)
				{
					char *last = strrchr(result, '/');
					*last = '\0';
				}

				if (strlen(current) > 0)
				{
					if (link[link_len - 1] != '/')
					{
						link[link_len] = '/';
						link[link_len + 1] = '\0';
					}
					
					snprintf(tmp, 2*PATH_MAX, "%s%s", link, current);
					strncpy(link, tmp, PATH_MAX);
				}

				strncpy(current, link, PATH_MAX);
				continue;
			}
			else 
			{
				report_error(result, token, errno);
				close(result_fd);
				return;
			}
		}
		
		snprintf(tmp, 2*PATH_MAX, "%s%s", result, token);
		strncpy(result, tmp, PATH_MAX);
		close(result_fd);
		result_fd = new_result_fd;
	}

	struct stat path_stat;
	fstat(result_fd, &path_stat);
	if (strlen(result) == 0 || S_ISDIR(path_stat.st_mode))
	{
		result[strlen(result) + 1] = '\0';
		result[strlen(result)] = '/';
	}

	close(result_fd);
	report_path(result);
}