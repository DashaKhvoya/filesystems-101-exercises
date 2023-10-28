#include <ext2fs/ext2_fs.h>
#include <ext2fs/ext2fs.h>
#include <fs_malloc.h>
#include <solution.h>
#include <unistd.h>

#include <errno.h>

struct ext2_fs
{
	int fd;
	struct ext2_super_block super;
	int inode_table_block;
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
	return block * fs->block_size;
}

int ext2_fs_init(struct ext2_fs **fs, int fd)
{
	struct ext2_fs *new_fs = fs_xmalloc(sizeof(struct ext2_fs));
	new_fs->fd = fd;

	int res = pread(new_fs->fd, &new_fs->super, SUPERBLOCK_SIZE, SUPERBLOCK_OFFSET);
	if (res == -1)
	{
		fs_xfree(new_fs);
		return -errno;
	}
	new_fs->block_size = EXT2_BLOCK_SIZE(&new_fs->super);
	// Check super MAGIC

	struct ext2_group_desc group;
	res = pread(new_fs->fd, &group, sizeof(group), get_offset(new_fs, new_fs->super.s_first_data_block + 1));
	if (res == -1)
	{
		fs_xfree(new_fs);
		return -errno;
	}
	// Check group hash

	new_fs->inode_table_block = group.bg_inode_table;
	*fs = new_fs;
	return 0;
}

void ext2_fs_free(struct ext2_fs *fs)
{
	fs_xfree(fs);
}

int ext2_blkiter_init(struct ext2_blkiter **i, struct ext2_fs *fs, int ino)
{
	struct ext2_blkiter *new_iter = fs_xmalloc(sizeof(struct ext2_blkiter));
	int res = pread(fs->fd, &new_iter->inode, fs->super.s_inode_size,
					get_offset(fs, fs->inode_table_block) + (ino - 1) * fs->super.s_inode_size);
	if (res == -1)
	{
		return -errno;
	}

	*i = new_iter;
	new_iter->curr = 0;
	new_iter->indirect_ptrs = NULL;
	new_iter->fs = fs;

	return 0;
}

int ext2_blkiter_next(struct ext2_blkiter *i, int *blkno)
{
	if (i->curr < EXT2_NDIR_BLOCKS)
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

	int indirect_ptr = i->inode.i_block[EXT2_IND_BLOCK];
	if (indirect_ptr == 0)
	{
		return 0;
	}
	if (i->indirect_ptrs == NULL)
	{
		i->indirect_ptrs = fs_xmalloc(i->fs->block_size * sizeof(int));
		int res = pread(i->fs->fd, i->indirect_ptrs, i->fs->block_size * sizeof(int), get_offset(i->fs, indirect_ptr));
		if (res == -1)
		{
			return -errno;
		}
		*blkno = indirect_ptr;
		return 1;
	}

	int ptr = i->indirect_ptrs[i->curr - EXT2_NDIR_BLOCKS];
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
