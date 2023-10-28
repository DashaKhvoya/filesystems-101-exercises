#include <ext2fs/ext2_fs.h>
#include <ext2fs/ext2fs.h>
#include <fs_malloc.h>
#include <solution.h>
#include <unistd.h>

#include <errno.h>

struct cached_block
{
	int block_id;
	int *block;
};

static void init_cached_block(struct cached_block *b);

struct ext2_fs
{
	int fd;
	struct ext2_super_block super;
	int block_size;
};

struct ext2_blkiter
{
	struct ext2_fs *fs;
	int inode_table_block;

	struct ext2_inode inode;
	int curr;

	struct cached_block indirect;

	struct cached_block dindirect;
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

	printf("inodes per group: %d\n", new_fs->super.s_inodes_per_group);
	printf("inodes count: %d, free count: %d\n", new_fs->super.s_inodes_count, new_fs->super.s_free_inodes_count);
	new_fs->block_size = EXT2_BLOCK_SIZE(&new_fs->super);
	// Check super MAGIC

	*fs = new_fs;
	return 0;
}

void ext2_fs_free(struct ext2_fs *fs)
{
	fs_xfree(fs);
}

int ext2_blkiter_init(struct ext2_blkiter **i, struct ext2_fs *fs, int ino)
{
	int group_id = (ino - 1) / fs->super.s_inodes_per_group;
	int ino_id = (ino - 1) % fs->super.s_inodes_per_group;

	int desc_per_block = fs->block_size / sizeof(struct ext2_group_desc);

	int blckno = fs->super.s_first_data_block + 1 + group_id / desc_per_block;
	int blkoff = (group_id % desc_per_block) * sizeof(struct ext2_group_desc);

	struct ext2_group_desc group;
	int res = pread(fs->fd, &group, sizeof(group), get_offset(fs, blckno) + blkoff);
	if (res == -1)
	{
		return -errno;
	}
	// Check group hash

	struct ext2_blkiter *new_iter = fs_xmalloc(sizeof(struct ext2_blkiter));
	new_iter->inode_table_block = group.bg_inode_table;

	res = pread(fs->fd, &new_iter->inode, sizeof(struct ext2_inode),
				get_offset(fs, new_iter->inode_table_block) + ino_id * fs->super.s_inode_size);
	if (res == -1)
	{
		return -errno;
	}

	*i = new_iter;
	new_iter->curr = 0;
	init_cached_block(&new_iter->indirect);
	init_cached_block(&new_iter->dindirect);
	new_iter->fs = fs;

	return 0;
}

static void init_cached_block(struct cached_block *b)
{
	b->block_id = 0;
	b->block = NULL;
}

static int get_cached_block(struct cached_block *b, int new_id, struct ext2_fs *fs)
{
	if (b->block && new_id == b->block_id)
	{
		return 0;
	}

	if (!b->block)
	{
		b->block = fs_xmalloc(fs->block_size);
	}

	int res = pread(fs->fd, b->block, fs->block_size, get_offset(fs, new_id));
	if (res == -1)
	{
		return -1;
	}

	b->block_id = new_id;
	return 1;
}

static int process_direct_block(struct ext2_blkiter *i, int *blkno, int id)
{
	int ptr = i->inode.i_block[id];
	if (ptr == 0)
	{
		return 0;
	}
	*blkno = ptr;
	i->curr++;

	return 1;
}

int ext2_blkiter_next(struct ext2_blkiter *i, int *blkno)
{
	int ptrs_in_block = i->fs->block_size / sizeof(int);

	int direct_to = EXT2_NDIR_BLOCKS;
	int indirect_from = direct_to, indirect_to = direct_to + ptrs_in_block;
	int dindirect_from = indirect_to, dindirect_to = indirect_to + ptrs_in_block * ptrs_in_block;

	if (i->curr < direct_to)
	{
		return process_direct_block(i, blkno, i->curr);
	}
	if (i->curr < indirect_to)
	{
		int is_updated = get_cached_block(&i->indirect, i->inode.i_block[EXT2_IND_BLOCK], i->fs);
		if (is_updated)
		{
			if (is_updated < 0)
			{
				return -errno;
			}
			*blkno = i->inode.i_block[EXT2_IND_BLOCK];
			return 1;
		}

		int ind_pos = i->curr - indirect_from;
		int ptr = i->indirect.block[ind_pos];
		if (ptr == 0)
		{
			return 0;
		}
		*blkno = ptr;
		i->curr++;
		return 1;
	}
	if (i->curr < dindirect_to)
	{
		int is_updated = get_cached_block(&i->dindirect, i->inode.i_block[EXT2_DIND_BLOCK], i->fs);
		if (is_updated)
		{
			if (is_updated < 0)
			{
				return -errno;
			}
			*blkno = i->inode.i_block[EXT2_DIND_BLOCK];
			return 1;
		}

		int dind_pos = (i->curr - dindirect_from) / ptrs_in_block;
		int ind_pos = (i->curr - dindirect_from) % ptrs_in_block;

		is_updated = get_cached_block(&i->indirect, i->dindirect.block[dind_pos], i->fs);
		if (is_updated)
		{
			if (is_updated < 0)
			{
				return -errno;
			}
			*blkno = i->dindirect.block[dind_pos];
			return 1;
		}

		int ptr = i->indirect.block[ind_pos];
		if (ptr == 0)
		{
			return 0;
		}
		*blkno = ptr;
		i->curr++;
		return 1;
	}

	return 0;
}

void ext2_blkiter_free(struct ext2_blkiter *i)
{
	if (i == NULL)
	{
		return;
	}
	fs_xfree(i->indirect.block);
	fs_xfree(i->dindirect.block);

	fs_xfree(i);
}
