#include <ext2fs/ext2fs.h>
#include <fs_malloc.h>
#include <solution.h>
#include <unistd.h>

#include <errno.h>

#define BLOCK_SIZE 1024

struct ext2_fs
{
	int fd;
	int inode_table;
	int block_size;
};

struct ext2_blkiter
{
	struct ext2_fs *fs;

	struct ext2_inode inode;
	int curr;

	int *indirect_ptrs;
};

static int get_offset(struct ext2_fs *fs, int block)
{
	return BLOCK_SIZE + (block - 1) * fs->block_size;
}

int ext2_fs_init(struct ext2_fs **fs, int fd)
{
	struct ext2_fs *new_fs = fs_xmalloc(sizeof(struct ext2_fs));
	new_fs->fd = fd;

	int res = lseek(new_fs->fd, BLOCK_SIZE, SEEK_SET);
	if (res == -1)
	{
		fs_xfree(new_fs);
		return -errno;
	}

	struct ext2_super_block super;
	res = read(new_fs->fd, &super, sizeof(super));
	if (res == -1)
	{
		fs_xfree(new_fs);
		return -errno;
	}
	new_fs->block_size = BLOCK_SIZE << super.s_log_block_size;
	// Check super MAGIC

	struct ext2_group_desc group;
	res = lseek(new_fs->fd, BLOCK_SIZE + new_fs->block_size, SEEK_SET);
	if (res == -1)
	{
		fs_xfree(new_fs);
		return -errno;
	}
	res = read(new_fs->fd, &group, sizeof(group));
	if (res == -1)
	{
		fs_xfree(new_fs);
		return -errno;
	}
	// Check group hash

	new_fs->inode_table = group.bg_inode_table;
	*fs = new_fs;
	return 0;
}

void ext2_fs_free(struct ext2_fs *fs)
{
	fs_xfree(fs);
}

int ext2_blkiter_init(struct ext2_blkiter **i, struct ext2_fs *fs, int ino)
{
	int res = lseek(fs->fd, get_offset(fs, fs->inode_table) + (ino - 1) * sizeof(struct ext2_inode), SEEK_SET);
	if (res == -1)
	{
		return -errno;
	}
	struct ext2_blkiter *new_iter = fs_xmalloc(sizeof(struct ext2_blkiter));
	res = read(fs->fd, &new_iter->inode, sizeof(struct ext2_inode));
	if (res == -1)
	{
		return errno;
	}

	*i = new_iter;
	new_iter->curr = 0;
	new_iter->indirect_ptrs = NULL;
	new_iter->fs = fs;

	return 0;
}

int ext2_blkiter_next(struct ext2_blkiter *i, int *blkno)
{
	if (i->curr < 12)
	{
		int ptr = i->inode.i_block[i->curr];
		if (ptr == 0)
		{
			return 0;
		}
		*blkno = ptr;
		i->curr++;
		return 1;
	}

	int indirect_ptr = i->inode.i_block[12];
	if (indirect_ptr == 0)
	{
		return 0;
	}
	if (i->indirect_ptrs == NULL)
	{
		int res = lseek(i->fs->fd, get_offset(i->fs, indirect_ptr), SEEK_SET);
		if (res == -1)
		{
			return -errno;
		}
		i->indirect_ptrs = fs_xmalloc(BLOCK_SIZE * sizeof(int));
		res = read(i->fs->fd, i->indirect_ptrs, BLOCK_SIZE * sizeof(int));
		if (res == -1)
		{
			return -errno;
		}
		*blkno = indirect_ptr;
		return 1;
	}

	int ptr = i->indirect_ptrs[i->curr - 12];
	if (ptr == 0)
	{
		return 0;
	}
	*blkno = ptr;
	i->curr++;

	return 1;
}

void ext2_blkiter_free(struct ext2_blkiter *i)
{
	if (i == NULL)
	{
		return;
	}
	if (i->indirect_ptrs != NULL)
	{
		fs_xfree(i->indirect_ptrs);
	}
	fs_xfree(i);
}
