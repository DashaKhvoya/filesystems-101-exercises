#include <solution.h>
#include <fs_malloc.h>

#include <errno.h>

struct ext2_fs
{
	/* implement me */
};

struct ext2_blkiter
{
	/* implement me */
};

int ext2_fs_init(struct ext2_fs **fs, int fd)
{
	/* implement me */
	(void) fs;
	(void) fd;
	return -EOPNOTSUPP;
}

void ext2_fs_free(struct ext2_fs *fs)
{
	/* implement me */
	(void) fs;
}

int ext2_blkiter_init(struct ext2_blkiter **i, struct ext2_fs *fs, int ino)
{
	/* implement me */
	(void) i;
	(void) fs;
	(void) ino;
	return -EOPNOTSUPP;
}

int ext2_blkiter_next(struct ext2_blkiter *i, int *blkno)
{
	/* implement me */
	(void) i;
	(void) blkno;
	return 0;
}

void ext2_blkiter_free(struct ext2_blkiter *i)
{
	/* implement me */
	(void) i;
}
