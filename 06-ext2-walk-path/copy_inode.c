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

int copy_file(int img, int inode_nr, int out)
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
