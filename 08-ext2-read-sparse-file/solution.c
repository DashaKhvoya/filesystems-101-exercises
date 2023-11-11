#include <ext2fs/ext2_fs.h>
#include <ext2fs/ext2fs.h>
#include <fs_malloc.h>
#include <solution.h>
#include <unistd.h>

#include <errno.h>

////////////////////////////////////
// Cached block
////////////////////////////////////

struct cached_block
{
	int block_id;
	int *block;
};

static void init_cached_block(struct cached_block *b, int block_size)
{
	b->block_id = 0;
	b->block = fs_xmalloc(block_size);
	memset(b->block, 0, block_size);
}

static int get_cached_block(struct cached_block *b, int new_id, int block_size, int fd)
{
	if (new_id == b->block_id)
	{
		return 0;
	}

	int res = pread(fd, b->block, block_size, block_size * new_id);
	if (res < 0)
	{
		return -1;
	}

	b->block_id = new_id;
	return 1;
}

////////////////////////////////////
// ext2
////////////////////////////////////

struct ext2_fs
{
	int fd;
	struct ext2_super_block super;
	int block_size;
};

static int get_offset(struct ext2_fs *fs, int block)
{
	return block * fs->block_size;
}

static int ext2_fs_init(struct ext2_fs **fs, int fd)
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

	*fs = new_fs;
	return 0;
}

static void ext2_fs_free(struct ext2_fs *fs)
{
	fs_xfree(fs);
}

////////////////////////////////////
// block iter
////////////////////////////////////

struct ext2_blkiter
{
	struct ext2_fs *fs;
	int inode_table_block;

	struct ext2_inode inode;
	int curr_block;
	int bytes_read;

	struct cached_block indirect;

	struct cached_block dindirect;
};

static int ext2_blkiter_init(struct ext2_blkiter **i, struct ext2_fs *fs, int ino)
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
	new_iter->curr_block = 0;
	new_iter->bytes_read = 0;
	init_cached_block(&new_iter->indirect, fs->block_size);
	init_cached_block(&new_iter->dindirect, fs->block_size);
	new_iter->fs = fs;

	return 0;
}

static int process_block(struct ext2_blkiter *i, int blockno, int *blkno, int *size)
{
	if (i->inode.i_size - i->bytes_read <= 0)
	{
		return 0;
	}

	int read_size = i->inode.i_size - i->bytes_read;
	if (read_size > i->fs->block_size)
	{
		read_size = i->fs->block_size;
	}

	*blkno = blockno;
	*size = read_size;
	i->curr_block++;
	i->bytes_read += read_size;

	return 1;
}

// blkno = 0 means sparse block
static int ext2_blkiter_next(struct ext2_blkiter *i, int *blkno, int *size)
{
	int ptrs_in_block = i->fs->block_size / sizeof(int);

	int direct_to = EXT2_NDIR_BLOCKS;
	int indirect_from = direct_to, indirect_to = direct_to + ptrs_in_block;
	int dindirect_from = indirect_to, dindirect_to = indirect_to + ptrs_in_block * ptrs_in_block;

	if (i->curr_block < direct_to)
	{
		return process_block(i, i->inode.i_block[i->curr_block], blkno, size);
	}
	if (i->curr_block < indirect_to)
	{
		int is_updated = get_cached_block(&i->indirect, i->inode.i_block[EXT2_IND_BLOCK], i->fs->block_size, i->fs->fd);
		if (is_updated < 0)
		{
			return -errno;
		}

		int ind_pos = i->curr_block - indirect_from;
		return process_block(i, i->indirect.block[ind_pos], blkno, size);
	}
	if (i->curr_block < dindirect_to)
	{
		int is_updated =
			get_cached_block(&i->dindirect, i->inode.i_block[EXT2_DIND_BLOCK], i->fs->block_size, i->fs->fd);
		if (is_updated < 0)
		{
			return -errno;
		}

		int dind_pos = (i->curr_block - dindirect_from) / ptrs_in_block;
		int ind_pos = (i->curr_block - dindirect_from) % ptrs_in_block;

		is_updated = get_cached_block(&i->indirect, i->dindirect.block[dind_pos], i->fs->block_size, i->fs->fd);
		if (is_updated < 0)
		{
			return -errno;
		}

		return process_block(i, i->indirect.block[ind_pos], blkno, size);
	}

	return 0;
}

static void ext2_blkiter_free(struct ext2_blkiter *i)
{
	if (i == NULL)
	{
		return;
	}
	fs_xfree(i->indirect.block);
	fs_xfree(i->dindirect.block);

	fs_xfree(i);
}

int dump_file(int img, int inode_nr, int out)
{
	struct ext2_fs *fs = NULL;
	int ret = ext2_fs_init(&fs, img);
	if (ret < 0)
	{
		return ret;
	}

	struct ext2_blkiter *iter = NULL;
	ret = ext2_blkiter_init(&iter, fs, inode_nr);
	if (ret < 0)
	{
		return ret;
	}

	int blkno = 0;
	int size = 0;
	char *zeroes = fs_xmalloc(fs->block_size);
	memset(zeroes, 0, fs->block_size);
	char *buf = fs_xmalloc(fs->block_size);

	while ((ret = ext2_blkiter_next(iter, &blkno, &size)) == 1)
	{
		if (blkno == 0)
		{
			if (write(out, zeroes, size) < 0)
			{
				ret = -errno;
				break;
			}
			continue;
		}
		if (pread(img, buf, size, fs->block_size * blkno) < 0)
		{
			ret = -errno;
			break;
		}
		if (write(out, buf, size) < 0)
		{
			ret = -errno;
			break;
		}
	}

	fs_xfree(zeroes);
	fs_xfree(buf);
	ext2_fs_free(fs);
	ext2_blkiter_free(iter);

	return ret;
}
