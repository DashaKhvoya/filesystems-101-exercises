#include <linux/limits.h>
#include <stdio.h>
#include <string.h>

#include <ext2fs/ext2_fs.h>
#include <ext2fs/ext2fs.h>
#include <fs_malloc.h>
#include <unistd.h>

#include <errno.h>

struct cached_block_1
{
	int block_id;
	int *block;
};

static void init_cached_block_1(struct cached_block_1 *b)
{
	b->block_id = 0;
	b->block = NULL;
}

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
	int curr_size;

	struct cached_block_1 indirect;
	struct cached_block_1 dindirect;
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
	new_iter->curr_size = 0;
	init_cached_block_1(&new_iter->indirect);
	init_cached_block_1(&new_iter->dindirect);
	new_iter->fs = fs;

	return 0;
}

static int get_cached_block_1(struct cached_block_1 *b, int new_id, struct ext2_fs *fs)
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
	*blkno = ptr;
	i->curr++;
	i->curr_size += i->fs->block_size;

	return 1;
}

static int ext2_blkiter_next(struct ext2_blkiter *i, int *blkno)
{
	if (i->curr_size >= (int)i->inode.i_size)
		return 0;

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
		int is_updated = get_cached_block_1(&i->indirect, i->inode.i_block[EXT2_IND_BLOCK], i->fs);
		if (is_updated < 0)
		{
			return -errno;
		}

		int ind_pos = i->curr - indirect_from;
		int ptr = i->indirect.block[ind_pos];
		*blkno = ptr;
		i->curr++;
		i->curr_size += i->fs->block_size;
		return 1;
	}
	if (i->curr < dindirect_to)
	{
		int is_updated = get_cached_block_1(&i->dindirect, i->inode.i_block[EXT2_DIND_BLOCK], i->fs);
		if (is_updated)
		{
			return -errno;
		}

		int dind_pos = (i->curr - dindirect_from) / ptrs_in_block;
		int ind_pos = (i->curr - dindirect_from) % ptrs_in_block;

		is_updated = get_cached_block_1(&i->indirect, i->dindirect.block[dind_pos], i->fs);
		if (is_updated)
		{
			return -errno;
		}

		int ptr = i->indirect.block[ind_pos];
		*blkno = ptr;
		i->curr++;
		i->curr_size += i->fs->block_size;
		return 1;
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

struct reporter
{
	char *block;
};

static void init_reporter(struct reporter *r, int block_size)
{
	r->block = fs_xmalloc(block_size);
}

static void free_reporter(struct reporter *r)
{
	fs_xfree(r->block);
}

static int find_inode_in_dir(struct reporter *r, int img, int blkno, int block_size, const char *name, int is_dir)
{
	int err = pread(img, r->block, block_size, block_size * blkno);
	if (err < 0)
	{
		return -errno;
	}

	for (int offset = 0; offset < block_size;)
	{
		struct ext2_dir_entry_2 *dir = (struct ext2_dir_entry_2 *)(r->block + offset);
		if (dir->inode == 0)
			break;

		// printf("strcmpy %.*s with %s\n", dir->name_len, dir->name, name);

		if (strlen(name) != dir->name_len || strncmp(name, dir->name, dir->name_len))
		{
			offset += dir->rec_len;
			continue;
		}

		if (is_dir && dir->file_type != EXT2_FT_DIR)
		{
			return -ENOTDIR;
		}
		if (!is_dir && dir->file_type != EXT2_FT_REG_FILE)
		{
			return -ENOENT;
		}

		return dir->inode;
	}

	return 0;
}

static int find_inode(int img, int inode_nr, const char *name, int is_dir)
{
	struct ext2_fs *fs = NULL;
	struct ext2_blkiter *i = NULL;
	int err = 0;
	int inode = 0;

	if ((err = ext2_fs_init(&fs, img)))
	{
		return -err;
	}
	if ((err = ext2_blkiter_init(&i, fs, inode_nr)))
	{
		return -err;
	}

	struct reporter r;
	init_reporter(&r, fs->block_size);

	for (;;)
	{
		int blkno;
		err = ext2_blkiter_next(i, &blkno);
		if (err < 0)
			break;
		else if (err > 0)
		{
			int ret = find_inode_in_dir(&r, img, blkno, fs->block_size, name, is_dir);
			if (ret < 0)
			{
				err = -ret;
				break;
			}
			if (ret > 0)
			{
				inode = ret;
				break;
			}
		}
		else
			break;
	}

	ext2_blkiter_free(i);
	ext2_fs_free(fs);
	free_reporter(&r);

	// printf("end\n");
	if (inode > 0)
	{
		return inode;
	}
	if (!err)
	{
		err = ENOENT;
	}

	return -err;
}

struct cached_block
{
	int block_id;
	int *block;
};

static void init_cached_block(struct cached_block *b)
{
	b->block_id = 0;
	b->block = NULL;
}

static int get_cached_block(struct cached_block *b, int new_id, int block_size, int fd)
{
	if (b->block && new_id == b->block_id)
	{
		return 0;
	}

	if (!b->block)
	{
		b->block = fs_xmalloc(block_size);
	}

	int res = pread(fd, b->block, block_size, block_size * new_id);
	if (res < 0)
	{
		return -1;
	}

	b->block_id = new_id;
	return 1;
}

struct copier
{
	void *buf;
	int buf_size;

	struct cached_block indr;
	struct cached_block dindr;
};

static void init_copier(struct copier *c, int size)
{
	c->buf = fs_xmalloc(size);
	c->buf_size = size;

	init_cached_block(&c->indr);
	init_cached_block(&c->dindr);
}

static void free_copier(struct copier *c)
{
	fs_xfree(c->buf);
	fs_xfree(c->indr.block);
	fs_xfree(c->dindr.block);
}

static int copy_block(struct copier *c, int src, int dest, int offset, int size)
{
	int copy_size = c->buf_size;
	if (size < copy_size)
	{
		copy_size = size;
	}

	int res = pread(src, c->buf, copy_size, offset);
	if (res < 0)
	{
		return -errno;
	}

	res = write(dest, c->buf, copy_size);
	if (res < 0)
	{
		return -errno;
	}

	return 0;
}

static int copy_file(int img, int inode_nr, int out)
{
	struct ext2_super_block super;
	int res = pread(img, &super, SUPERBLOCK_SIZE, SUPERBLOCK_OFFSET);
	if (res < 0)
	{
		return -errno;
	}

	int block_size = EXT2_BLOCK_SIZE(&super);

	int group_id = (inode_nr - 1) / super.s_inodes_per_group;
	int ino_id = (inode_nr - 1) % super.s_inodes_per_group;
	int desc_per_block = block_size / sizeof(struct ext2_group_desc);

	int blckno = super.s_first_data_block + 1 + group_id / desc_per_block;
	int blkoff = (group_id % desc_per_block) * sizeof(struct ext2_group_desc);

	struct ext2_group_desc group;
	res = pread(img, &group, sizeof(group), blckno * block_size + blkoff);
	if (res < 0)
	{
		return -errno;
	}

	struct ext2_inode inode;
	res =
		pread(img, &inode, sizeof(struct ext2_inode), block_size * group.bg_inode_table + ino_id * super.s_inode_size);
	if (res < 0)
	{
		return -errno;
	}

	int ptrs_in_block = block_size / sizeof(int);

	struct copier copier;
	init_copier(&copier, block_size);
	int copy_left = inode.i_size;
	for (int i = 0; i < EXT2_NDIR_BLOCKS && copy_left > 0; i++)
	{
		int err = copy_block(&copier, img, out, block_size * inode.i_block[i], copy_left);
		if (err)
		{
			free_copier(&copier);
			return err;
		}
		copy_left -= block_size;
	}

	for (int i = 0; i < ptrs_in_block && copy_left > 0; i++)
	{
		int res = get_cached_block(&copier.indr, inode.i_block[EXT2_IND_BLOCK], block_size, img);
		if (res < 0)
		{
			free_copier(&copier);
			return -errno;
		}
		int err = copy_block(&copier, img, out, block_size * copier.indr.block[i], copy_left);
		if (err)
		{
			free_copier(&copier);
			return err;
		}
		copy_left -= block_size;
	}

	for (int indr_id = 0; indr_id < ptrs_in_block && copy_left > 0; indr_id++)
	{
		int res = get_cached_block(&copier.dindr, inode.i_block[EXT2_DIND_BLOCK], block_size, img);
		if (res < 0)
		{
			free_copier(&copier);
			return -errno;
		}
		for (int i = 0; i < ptrs_in_block && copy_left > 0; i++)
		{
			int res = get_cached_block(&copier.indr, copier.dindr.block[indr_id], block_size, img);
			if (res < 0)
			{
				free_copier(&copier);
				return -errno;
			}
			int err = copy_block(&copier, img, out, block_size * copier.indr.block[i], copy_left);
			if (err)
			{
				free_copier(&copier);
				return err;
			}
			copy_left -= block_size;
		}
	}

	free_copier(&copier);
	return 0;
}

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
	if (strlen(path) + 1 > PATH_MAX)
	{
		return -ENAMETOOLONG;
	}

	memcpy(path_copy, path, strlen(path) + 1);
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
