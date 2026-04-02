#include "path_tree.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>

static int node_height(const PathTreeNode *node) {
    return node == NULL ? 0 : node->height;
}

static int max_int(int a, int b) {
    return a > b ? a : b;
}

static void update_height(PathTreeNode *node) {
    node->height = 1 + max_int(node_height(node->left), node_height(node->right));
}

static PathTreeNode *rotate_right(PathTreeNode *node) {
    PathTreeNode *left = node->left;
    node->left = left->right;
    left->right = node;
    update_height(node);
    update_height(left);
    return left;
}

static PathTreeNode *rotate_left(PathTreeNode *node) {
    PathTreeNode *right = node->right;
    node->right = right->left;
    right->left = node;
    update_height(node);
    update_height(right);
    return right;
}

static int compare_records(const ChangeRecord *records, size_t lhs, size_t rhs) {
    return strcmp(records[lhs].path, records[rhs].path);
}

static PathTreeNode *rebalance(PathTreeNode *node) {
    update_height(node);
    int balance = node_height(node->left) - node_height(node->right);
    if (balance > 1) {
        if (node_height(node->left->left) < node_height(node->left->right)) {
            node->left = rotate_left(node->left);
        }
        return rotate_right(node);
    }
    if (balance < -1) {
        if (node_height(node->right->right) < node_height(node->right->left)) {
            node->right = rotate_right(node->right);
        }
        return rotate_left(node);
    }
    return node;
}

static int insert_node(PathTreeNode **root, const ChangeRecord *records, size_t record_index) {
    if (*root == NULL) {
        PathTreeNode *node = calloc(1, sizeof(*node));
        if (node == NULL) {
            return ENOMEM;
        }
        node->record_index = record_index;
        node->height = 1;
        *root = node;
        return 0;
    }

    int cmp = compare_records(records, record_index, (*root)->record_index);
    int rc = 0;
    if (cmp < 0) {
        rc = insert_node(&(*root)->left, records, record_index);
    } else if (cmp > 0) {
        rc = insert_node(&(*root)->right, records, record_index);
    } else {
        (*root)->record_index = record_index;
        return 0;
    }
    if (rc != 0) {
        return rc;
    }
    *root = rebalance(*root);
    return 0;
}

int path_tree_insert(PathTreeNode **root, const ChangeRecord *records, size_t record_index) {
    return insert_node(root, records, record_index);
}

int path_tree_inorder(const PathTreeNode *root, path_tree_visit_fn visitor, void *user_data) {
    if (root == NULL) {
        return 0;
    }
    int rc = path_tree_inorder(root->left, visitor, user_data);
    if (rc != 0) {
        return rc;
    }
    rc = visitor(root->record_index, user_data);
    if (rc != 0) {
        return rc;
    }
    return path_tree_inorder(root->right, visitor, user_data);
}

void path_tree_destroy(PathTreeNode *root) {
    if (root == NULL) {
        return;
    }
    path_tree_destroy(root->left);
    path_tree_destroy(root->right);
    free(root);
}
