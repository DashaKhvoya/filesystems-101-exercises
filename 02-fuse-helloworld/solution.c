#include <sys/types.h>
#include <sys/stat.h>
#include "solution.h"
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <fuse3/fuse.h>

#define BUF_SIZE 1000

static int readdir_custom(const char *path, void *buffer, fuse_fill_dir_t filler, off_t offset,
						  struct fuse_file_info *fi, enum fuse_readdir_flags fuse_flags)
{
	(void)path;
	(void)offset;
	(void)fi;
	(void)fuse_flags;

	filler(buffer, ".", NULL, 0, 0);
	filler(buffer, "..", NULL, 0, 0);
	filler(buffer, "hello", NULL, 0, 0);

	return 0;
}

static int read_custom(const char *path, char *buffer, size_t size, off_t offset,
					   struct fuse_file_info *fi)
{
	(void)fi;
	(void)path;

	pid_t current_pid = fuse_get_context()->pid;
	char text[BUF_SIZE];

	snprintf(text, BUF_SIZE, "hello, %d\n", current_pid);

	memcpy(buffer, text + offset, size);
	if (offset < (int)strlen(text))
	{
		return (int)strlen(text) - offset;
	}
	return 0;
}

static int open_custom(const char *path, struct fuse_file_info *fi)
{
	(void)path;

	if ((fi->flags & O_ACCMODE) != O_RDONLY)
		return -EROFS;

	return 0;
}

static int getattr_custom(const char *path, struct stat *st,
						  struct fuse_file_info *fi)
{
	(void)fi;

	st->st_uid = getuid();
	st->st_gid = getgid();
	st->st_atime = time(NULL);
	st->st_mtime = time(NULL);

	st->st_mode = S_IFREG | 0444;
	st->st_nlink = 1;
	st->st_size = 1;

	return 0;
}

static int write_custom(const char *req, const char *buffer, size_t size,
						off_t offset, struct fuse_file_info *fi)
{
	(void)req;
	(void)buffer;
	(void)size;
	(void)offset;
	(void)fi;
	return EROFS;
}

static int create_custom(const char *path, mode_t mode, struct fuse_file_info *fi)
{
	(void)path;
	(void)mode;
	(void)fi;
	return EROFS;
}

static const struct fuse_operations hellofs_ops = {
	.readdir = readdir_custom,
	.read = read_custom,
	.open = open_custom,
	.getattr = getattr_custom,
	.write = write_custom,
	.create = create_custom,
};

int helloworld(const char *mntp)
{
	char *argv[] = {"exercise", "-f", (char *)mntp, NULL};
	return fuse_main(3, argv, &hellofs_ops, NULL);
}
