#include <solution.h>

#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <err.h>

int main(int argc, char **argv)
{
	if (argc != 3) {
		fprintf(stderr, "use: ./a.out <img-file-name> <inode-nr>\n");
		return 1;
	}

	int img = open(argv[1], O_RDONLY);
	if (img < 0)
		errx(1, "open(img) failed");
	int ino = atoi(argv[2]);

	struct ext2_fs *fs = NULL;
	struct ext2_blkiter *i = NULL;
	int r;

	if ((r = ext2_fs_init(&fs, img)))
		errx(1, "ext2_fs_init() failed");
	if ((r = ext2_blkiter_init(&i, fs, ino)))
		errx(1, "ext2_blkiter_init() failed");

	for (;;) {
		int blkno;
		r = ext2_blkiter_next(i, &blkno);
		if (r < 0)
			errx(1, "ext2_blkiter_next() failed");
		else if (r > 0)
			printf("%i\n", blkno);
		else
			break;
	}

	ext2_blkiter_free(i);
	ext2_fs_free(fs);
	return 0;
}
