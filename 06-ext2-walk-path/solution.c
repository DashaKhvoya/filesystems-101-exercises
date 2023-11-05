#include "solution.h"
#include <errno.h>
#include <linux/limits.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

int dump_file(int img, const char *path, int out)
{
	(void)img;
	(void)path;
	(void)out;

	if (path[0] != '/')
	{
		return -ENOENT;
	}

	char path_copy[PATH_MAX];
	if (strlen(path) >= PATH_MAX)
	{
		return -ENAMETOOLONG;
	}

	strncpy(path_copy, path, strlen(path) + 1);
	char *rest = NULL;
	char *token;

	int curr_inode = 2;
	for (token = strtok_r(path_copy, "/", &rest); token != NULL; token = strtok_r(NULL, "/", &rest))
	{
		if (rest[0] == '\0')
		{
			int ret = find_inode(img, curr_inode, token, 0);
			if (ret < 0)
			{
				// printf("cant find file %s: %s\n", token, strerror(-ret));
				return ret;
			}
			if (ret == 0)
			{
				// printf("cant find file  %s: %s\n", token, strerror(-ENOENT));
				return -ENOENT;
			}
			ret = copy_file(img, ret, out);
			if (ret < 0)
			{
				// printf("cant copy %s: %s\n", token, strerror(-ret));
				return ret;
			}
			return 0;
		}

		int ret = find_inode(img, curr_inode, token, 1);
		if (ret < 0)
		{
			// printf("cant find dir %s: %s\n", token, strerror(-ret));
			return ret;
		}
		if (ret == 0)
		{
			// printf("cant find dir  %s: %s\n", token, strerror(-ENOENT));
			return -ENOENT;
		}
		curr_inode = ret;
	}

	return 0;
}
