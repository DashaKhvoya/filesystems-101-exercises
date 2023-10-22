#include <solution.h>
#include <liburing.h>
#include <assert.h>
#include <stdio.h>
#include "../stdlib/fs_malloc.h"

static const int QUEUE_BLOCK_SIZE = 256 * 1024;
// #define BLOCK_SIZE 3
#define QUEUE_SIZE 4

struct request
{
	int is_read;
	size_t size;
	long offset;
	struct iovec iov;
};

// Добавляем чтение, но без отправки submit
static int add_read(struct io_uring *ring, long size, long offset, int fd)
{
	struct request *data = fs_xmalloc(size + sizeof(struct request));

	struct io_uring_sqe *sqe = io_uring_get_sqe(ring);
	assert(sqe);

	data->is_read = 1;
	data->offset = offset;
	data->size = size;

	data->iov.iov_base = data + 1;
	data->iov.iov_len = size;

	io_uring_prep_readv(sqe, fd, &data->iov, 1, offset);
	io_uring_sqe_set_data(sqe, data);
	return 0;
}

// Планируем запись
static void add_write(struct io_uring *ring, struct request *data, int fd)
{
	struct io_uring_sqe *sqe = io_uring_get_sqe(ring);
	assert(sqe);

	data->is_read = 0;

	io_uring_prep_write(sqe, fd, data + 1, data->size, data->offset);
	io_uring_sqe_set_data(sqe, data);
	io_uring_submit(ring);
}

static int copy_queue(int in, int out, long size, struct io_uring *ring)
{
	printf("size = %ld\n", size);
	// Сколько нужно считать и записать
	int read_left = size;
	int write_left = size;
	int read_offset = 0;

	// Количество записей и чтений
	int read_count = 0;
	int write_count = 0;

	while (write_left || read_left)
	{
		int launched = 0;
		// Запускаем 4 чтения
		while (read_left)
		{
			long curr_size = read_left;

			if (read_count + write_count >= QUEUE_SIZE)
			{
				break;
			}
			if (curr_size > QUEUE_BLOCK_SIZE)
			{
				curr_size = QUEUE_BLOCK_SIZE;
			}

			printf("add read\n");
			int err = add_read(ring, curr_size, read_offset, in);
			if (err)
			{
				return err;
			}

			read_left -= curr_size;
			read_offset += curr_size;
			read_count++;
			launched = 1;
		}

		if (launched)
		{
			printf("submit reads\n");
			int err = io_uring_submit(ring);
			if (err < 0)
			{
				printf("submit err: %d\n", err);
				return err;
			}
		}

		int wait = 1;
		while (write_left)
		{
			struct io_uring_cqe *cqe;
			struct request *request;
			if (wait)
			{
				printf("wait for available\n");
				int err = io_uring_wait_cqe(ring, &cqe);
				if (err)
				{
					return err;
				}
				wait = 0;
			}
			else
			{
				printf("get if ready\n");
				int err = io_uring_peek_cqe(ring, &cqe);
				if (err == -EAGAIN)
				{
					// Больше не можем запустить чтений, очередь полна либо можно запусить еще чтения
					cqe = NULL;
					break;
				}
				else if (err)
				{
					return err;
				}
			}

			if (!cqe)
			{
				break;
			}

			request = io_uring_cqe_get_data(cqe);
			if (request->is_read)
			{
				printf("add write\n");
				add_write(ring, request, out);
				write_left -= request->size;
				read_count--;
				write_count++;
			}
			else
			{
				fs_xfree(request);
				write_count--;
			}
			io_uring_cqe_seen(ring, cqe);
		}
	}

	while (write_count)
	{
		struct io_uring_cqe *cqe;
		struct request *request;
		printf("wait at the end\n");
		int err = io_uring_wait_cqe(ring, &cqe);
		if (err)
		{
			return err;
		}

		request = io_uring_cqe_get_data(cqe);
		fs_xfree(request);
		write_count--;
		io_uring_cqe_seen(ring, cqe);
	}

	return 0;
}

int copy(int in, int out)
{
	(void)out;

	struct io_uring ring;
	int err = io_uring_queue_init(QUEUE_SIZE, &ring, 0);
	if (err)
	{
		return err;
	}

	struct stat st;
	err = fstat(in, &st);
	if (err)
	{
		return -errno;
	}

	long size = st.st_size;

	err = copy_queue(in, out, size, &ring);
	if (err)
	{
		return err;
	}

	return 0;
}
