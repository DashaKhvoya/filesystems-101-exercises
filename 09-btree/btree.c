#include <solution.h>
#include <stdio.h>
#include <stdlib.h>
#include <fs_malloc.h>

// -------------------------------------------
// BTree node
// -------------------------------------------

struct btree_node
{
	bool is_leaf;
	unsigned int num_keys;
	int *key;
	struct btree_node **children;
};

static struct btree_node *btree_node_alloc(unsigned int L, bool is_leaf)
{
	struct btree_node *node = fs_xmalloc(sizeof(*node));
	node->is_leaf = is_leaf;
	node->num_keys = 0;
	node->key = fs_xmalloc((2 * L - 1) * sizeof(int));
	node->children = fs_xmalloc((2 * L) * sizeof(struct btree_node *));

	return node;
}

static void btree_node_free(struct btree_node *node)
{
	if (node == NULL)
	{
		return;
	}

	if (!node->is_leaf)
	{
		for (unsigned int i = 0; i <= node->num_keys; ++i)
		{
			btree_node_free(node->children[i]);
		}
	}

	fs_xfree(node->key);
	fs_xfree(node->children);
	fs_xfree(node);
}

static bool btree_node_contains(struct btree_node *node, int x)
{
	int index = node->num_keys - 1;
	while (index >= 0 && x < node->key[index])
	{
		index -= 1;
	}

	if (index >= 0 && node->key[index] == x)
	{
		return true;
	}

	if (node->is_leaf)
	{
		return false;
	}

	return btree_node_contains(node->children[index + 1], x);
}

static void btree_node_simple_delete(struct btree_node *node, int x)
{
	int index = 0;
	while (x > node->key[index])
	{
		index += 1;
	}

	for (unsigned int i = index; i < node->num_keys - 1; ++i)
	{
		node->key[i] = node->key[i + 1];
	}

	node->num_keys -= 1;
}

static int btree_node_find_left_key(struct btree_node *node)
{
	if (node->is_leaf)
	{
		return node->key[0];
	}

	return btree_node_find_left_key(node->children[0]);
}

static int btree_node_find_right_key(struct btree_node *node)
{
	if (node->is_leaf)
	{
		return node->key[node->num_keys - 1];
	}

	return btree_node_find_right_key(node->children[node->num_keys]);
}

static void btree_node_clockwise_rotation(struct btree_node *parent, unsigned int child_index)
{
	struct btree_node *node = parent->children[child_index];
	struct btree_node *left = parent->children[child_index - 1];

	for (int i = node->num_keys - 1; i >= 0; --i)
	{
		node->key[i + 1] = node->key[i];
		if (!node->is_leaf)
		{
			node->children[i + 2] = node->children[i + 1];
		}
	}
	node->key[0] = parent->key[child_index - 1];
	if (!node->is_leaf)
	{
		node->children[1] = node->children[0];
		node->children[0] = left->children[left->num_keys];
	}
	node->num_keys += 1;

	parent->key[child_index - 1] = left->key[left->num_keys - 1];
	left->num_keys -= 1;
}

static void btree_node_counterclockwise_rotation(struct btree_node *parent, unsigned int child_index)
{
	struct btree_node *node = parent->children[child_index];
	struct btree_node *right = parent->children[child_index + 1];

	node->key[node->num_keys] = parent->key[child_index];
	if (!node->is_leaf)
	{
		node->children[node->num_keys + 1] = right->children[0];
	}
	node->num_keys += 1;

	parent->key[child_index] = right->key[0];

	for (unsigned int i = 0; i < right->num_keys - 1; ++i)
	{
		right->key[i] = right->key[i + 1];
		if (!node->is_leaf)
		{
			right->children[i] = right->children[i + 1];
		}
	}
	if (!node->is_leaf)
	{
		right->children[right->num_keys - 1] = right->children[right->num_keys];
	}
	right->num_keys -= 1;
}

// -------------------------------------------
// BTree
// -------------------------------------------

struct btree
{
	unsigned int L;
	struct btree_node *root;
};

struct btree *btree_alloc(unsigned int L)
{
	struct btree *tree = fs_xmalloc(sizeof(*tree));
	tree->L = L + 1;
	tree->root = btree_node_alloc(L + 1, true);
	return tree;
}

void btree_free(struct btree *t)
{
	btree_node_free(t->root);
	fs_xfree(t);
}

static void btree_split_child(struct btree_node *parent, int child_index, struct btree *t)
{
	struct btree_node *child = parent->children[child_index];
	struct btree_node *new_node = btree_node_alloc(t->L, child->is_leaf);

	new_node->num_keys = t->L - 1;
	for (unsigned int i = 0; i < t->L - 1; ++i)
	{
		new_node->key[i] = child->key[i + t->L];
	}
	if (!child->is_leaf)
	{
		for (unsigned int i = 0; i < t->L; ++i)
		{
			new_node->children[i] = child->children[i + t->L];
		}
	}

	child->num_keys = t->L - 1;

	for (int i = parent->num_keys - 1; i >= child_index; --i)
	{
		parent->key[i + 1] = parent->key[i];
	}
	parent->key[child_index] = child->key[t->L - 1];
	for (int i = parent->num_keys; i >= child_index + 1; --i)
	{
		parent->children[i + 1] = parent->children[i];
	}
	parent->children[child_index + 1] = new_node;
	parent->num_keys += 1;
}

static void btree_insert_nonfull(struct btree_node *node, int x, struct btree *t)
{
	int index = node->num_keys - 1;

	if (node->is_leaf)
	{
		while (index >= 0 && x < node->key[index])
		{
			node->key[index + 1] = node->key[index];
			index -= 1;
		}

		node->key[index + 1] = x;
		node->num_keys += 1;
	}
	else
	{
		while (index >= 0 && x < node->key[index])
		{
			index -= 1;
		}

		if (node->children[index + 1]->num_keys == 2 * t->L - 1)
		{
			btree_split_child(node, index + 1, t);
			if (x > node->key[index + 1])
			{
				index += 1;
			}
		}
		btree_insert_nonfull(node->children[index + 1], x, t);
	}
}

void btree_insert(struct btree *t, int x)
{
	if (btree_contains(t, x))
	{
		return;
	}

	if (t->root->num_keys == 2 * t->L - 1)
	{
		struct btree_node *new_root = btree_node_alloc(t->L, false);
		new_root->children[0] = t->root;
		t->root = new_root;
		btree_split_child(new_root, 0, t);
		btree_insert_nonfull(new_root, x, t);
	}
	else
	{
		btree_insert_nonfull(t->root, x, t);
	}
}

bool btree_contains(struct btree *t, int x)
{
	return btree_node_contains(t->root, x);
}

static void btree_merge(struct btree_node *node, unsigned int index)
{
	struct btree_node *left = node->children[index];
	struct btree_node *right = node->children[index + 1];

	left->key[left->num_keys] = node->key[index];
	if (!left->is_leaf)
	{
		left->children[left->num_keys + 1] = right->children[0];
	}
	left->num_keys += 1;
	for (unsigned int i = 0; i < right->num_keys; ++i)
	{
		left->key[left->num_keys + i] = right->key[i];
		if (!left->is_leaf)
		{
			left->children[left->num_keys + i + 1] = right->children[i + 1];
		}
	}
	left->num_keys += right->num_keys;

	fs_xfree(right->key);
	fs_xfree(right->children);
	fs_xfree(right);

	for (unsigned int i = index; i < node->num_keys - 1; ++i)
	{
		node->key[i] = node->key[i + 1];
		node->children[i + 1] = node->children[i + 2];
	}
	node->num_keys -= 1;
}

static void btree_merge_root(struct btree *t)
{
	struct btree_node *left = t->root->children[0];
	struct btree_node *right = t->root->children[1];

	left->key[left->num_keys] = t->root->key[0];
	if (!left->is_leaf)
	{
		left->children[left->num_keys + 1] = right->children[0];
	}
	left->num_keys += 1;
	for (unsigned int i = 0; i < right->num_keys; ++i)
	{
		left->key[left->num_keys + i] = right->key[i];
		if (!left->is_leaf)
		{
			left->children[left->num_keys + i + 1] = right->children[i + 1];
		}
	}
	left->num_keys += right->num_keys;

	fs_xfree(right->key);
	fs_xfree(right->children);
	fs_xfree(right);

	fs_xfree(t->root->key);
	fs_xfree(t->root->children);
	fs_xfree(t->root);

	t->root = left;
}

static void btree_delete_from_ok_node(struct btree_node *node, int x, struct btree *t)
{
	if (node->is_leaf)
	{
		btree_node_simple_delete(node, x);
	}
	else
	{
		int x_index = -1;
		bool is_in_keys = false;
		for (unsigned int i = 0; i < node->num_keys; ++i)
		{
			if (node->key[i] == x)
			{
				x_index = i;
				is_in_keys = true;
				break;
			}
		}

		if (is_in_keys)
		{
			if (node->children[x_index]->num_keys >= t->L)
			{
				int right_elem = btree_node_find_right_key(node->children[x_index]);
				node->key[x_index] = right_elem;
				btree_delete_from_ok_node(node->children[x_index], right_elem, t);
			}
			else if (node->children[x_index + 1]->num_keys >= t->L)
			{
				int left_elem = btree_node_find_left_key(node->children[x_index + 1]);
				node->key[x_index] = left_elem;
				btree_delete_from_ok_node(node->children[x_index + 1], left_elem, t);
			}
			else
			{
				btree_merge(node, x_index);
				btree_delete_from_ok_node(node->children[x_index], x, t);
			}
		}
		else
		{
			unsigned int index = 0;
			while (index < node->num_keys && x > node->key[index])
			{
				index += 1;
			}

			struct btree_node *sub_node = node->children[index];
			if (sub_node->num_keys >= t->L)
			{
				btree_delete_from_ok_node(sub_node, x, t);
			}
			else
			{
				if ((int)index - 1 >= 0 && node->children[index - 1]->num_keys >= t->L)
				{
					btree_node_clockwise_rotation(node, index);
					btree_delete_from_ok_node(sub_node, x, t);
				}
				else if (index + 1 <= node->num_keys && node->children[index + 1]->num_keys >= t->L)
				{
					btree_node_counterclockwise_rotation(node, index);
					btree_delete_from_ok_node(sub_node, x, t);
				}
				else
				{
					if ((int)index - 1 >= 0)
					{
						btree_merge(node, index - 1);
						btree_delete_from_ok_node(node->children[index - 1], x, t);
					}
					else
					{
						btree_merge(node, index);
						btree_delete_from_ok_node(node->children[index], x, t);
					}
				}
			}
		}
	}
}

void btree_delete(struct btree *t, int x)
{
	if (!btree_contains(t, x))
	{
		return;
	}

	if (t->root->num_keys > 1)
	{
		btree_delete_from_ok_node(t->root, x, t);
	}
	else if (t->root->is_leaf)
	{
		btree_node_simple_delete(t->root, x);
	}
	else if (t->root->children[0]->num_keys < t->L && t->root->children[1]->num_keys < t->L)
	{
		btree_merge_root(t);
		btree_delete_from_ok_node(t->root, x, t);
	}
	else
	{
		if (t->root->key[0] == x)
		{
			if (t->root->children[0]->num_keys >= t->L)
			{
				int right_elem = btree_node_find_right_key(t->root->children[0]);
				t->root->key[0] = right_elem;
				btree_delete_from_ok_node(t->root->children[0], right_elem, t);
			}
			else
			{
				int left_elem = btree_node_find_left_key(t->root->children[1]);
				t->root->key[0] = left_elem;
				btree_delete_from_ok_node(t->root->children[1], left_elem, t);
			}
		}
		else
		{
			if (x < t->root->key[0])
			{
				if (t->root->children[0]->num_keys >= t->L)
				{
					btree_delete_from_ok_node(t->root->children[0], x, t);
				}
				else
				{
					btree_node_counterclockwise_rotation(t->root, 0);
					btree_delete_from_ok_node(t->root->children[0], x, t);
				}
			}
			else
			{
				if (t->root->children[1]->num_keys >= t->L)
				{
					btree_delete_from_ok_node(t->root->children[1], x, t);
				}
				else
				{
					btree_node_clockwise_rotation(t->root, 1);
					btree_delete_from_ok_node(t->root->children[1], x, t);
				}
			}
		}
	}
}

// -------------------------------------------
// BTree iterator
// -------------------------------------------

struct btree_iter
{
	int *indexes;
	struct btree_node **levels;
	struct btree *t;
	int curr_level;
};

struct btree_iter *btree_iter_start(struct btree *t)
{
	struct btree_iter *i = fs_xmalloc(sizeof(*i));
	i->curr_level = 0;
	i->t = t;

	int levels = 1;
	struct btree_node *curr_node = t->root;
	while (!curr_node->is_leaf)
	{
		curr_node = curr_node->children[0];
		levels++;
	}

	i->levels = fs_xmalloc(sizeof(struct btree_node *) * levels);
	i->indexes = fs_xmalloc(sizeof(int) * levels);

	i->indexes[0] = 0;
	i->levels[0] = t->root;

	return i;
}

void btree_iter_end(struct btree_iter *i)
{
	fs_xfree(i->levels);
	fs_xfree(i->indexes);
	fs_xfree(i);
}

bool btree_iter_next(struct btree_iter *i, int *x)
{
	struct btree_node *curr_node = i->levels[i->curr_level];
	int children_index = i->indexes[i->curr_level];
	while (!curr_node->is_leaf)
	{
		curr_node = curr_node->children[children_index];
		i->curr_level++;
		i->levels[i->curr_level] = curr_node;
		i->indexes[i->curr_level] = 0;

		children_index = 0;
	}

	int curr_index = i->indexes[i->curr_level];
	if (curr_index < (int)curr_node->num_keys)
	{
		*x = curr_node->key[curr_index];
		i->indexes[i->curr_level]++;
		return true;
	}

	while (i->curr_level > 0)
	{
		i->curr_level--;

		curr_node = i->levels[i->curr_level];
		curr_index = i->indexes[i->curr_level];

		if (curr_index < (int)curr_node->num_keys)
		{
			*x = curr_node->key[curr_index];
			i->indexes[i->curr_level]++;
			return true;
		}
	}

	return false;
}
