#ifndef VERDIFF_PATH_TREE_H
#define VERDIFF_PATH_TREE_H

#include "common.h"

struct PathTreeNode {
    size_t record_index;
    int height;
    struct PathTreeNode *left;
    struct PathTreeNode *right;
};

typedef int (*path_tree_visit_fn)(size_t record_index, void *user_data);

int path_tree_insert(PathTreeNode **root, const ChangeRecord *records, size_t record_index);
int path_tree_inorder(const PathTreeNode *root, path_tree_visit_fn visitor, void *user_data);
void path_tree_destroy(PathTreeNode *root);

#endif
