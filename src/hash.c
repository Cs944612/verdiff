#include "hash.h"

#include "xxhash.h"

#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

static int hash_file_mmap(int fd, off_t size, uint64_t *hash_out) {
    if (size == 0) {
        *hash_out = XXH3_64bits(NULL, 0);
        return 0;
    }

    void *mapping = mmap(NULL, (size_t)size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (mapping == MAP_FAILED) {
        return errno;
    }

    *hash_out = XXH3_64bits(mapping, (size_t)size);
    munmap(mapping, (size_t)size);
    return 0;
}

static int hash_file_stream(int fd, size_t buffer_size, uint64_t *hash_out) {
    XXH3_state_t *state = XXH3_createState();
    if (state == NULL) {
        return ENOMEM;
    }
    XXH3_64bits_reset(state);

    unsigned char *buffer = malloc(buffer_size);
    if (buffer == NULL) {
        XXH3_freeState(state);
        return ENOMEM;
    }

    int rc = 0;
    for (;;) {
        ssize_t nread = read(fd, buffer, buffer_size);
        if (nread < 0) {
            rc = errno;
            break;
        }
        if (nread == 0) {
            break;
        }
        XXH_errorcode err = XXH3_64bits_update(state, buffer, (size_t)nread);
        if (err != XXH_OK) {
            rc = EIO;
            break;
        }
    }

    if (rc == 0) {
        *hash_out = XXH3_64bits_digest(state);
    }
    free(buffer);
    XXH3_freeState(state);
    return rc;
}

int hash_file_xxh3(const char *path, size_t buffer_size, size_t mmap_threshold, uint64_t *hash_out) {
    int fd = open(path, O_RDONLY);
    if (fd < 0) {
        return errno;
    }

    struct stat st;
    if (fstat(fd, &st) != 0) {
        int saved = errno;
        close(fd);
        return saved;
    }

    int rc = 0;
    if ((size_t)st.st_size <= mmap_threshold) {
        rc = hash_file_mmap(fd, st.st_size, hash_out);
        if (rc != 0) {
            if (lseek(fd, 0, SEEK_SET) < 0) {
                rc = errno;
            } else {
                rc = hash_file_stream(fd, buffer_size, hash_out);
            }
        }
    } else {
        rc = hash_file_stream(fd, buffer_size, hash_out);
    }

    close(fd);
    return rc;
}

int vd_compare_binary_files(const char *path_a, const char *path_b, size_t buffer_size, bool *equal_out) {
    int fd_a = open(path_a, O_RDONLY);
    if (fd_a < 0) {
        return errno;
    }
    int fd_b = open(path_b, O_RDONLY);
    if (fd_b < 0) {
        int saved = errno;
        close(fd_a);
        return saved;
    }

    unsigned char *buf_a = malloc(buffer_size);
    unsigned char *buf_b = malloc(buffer_size);
    if (buf_a == NULL || buf_b == NULL) {
        free(buf_a);
        free(buf_b);
        close(fd_a);
        close(fd_b);
        return ENOMEM;
    }

    int rc = 0;
    *equal_out = true;
    for (;;) {
        ssize_t read_a = read(fd_a, buf_a, buffer_size);
        if (read_a < 0) {
            rc = errno;
            break;
        }
        ssize_t read_b = read(fd_b, buf_b, buffer_size);
        if (read_b < 0) {
            rc = errno;
            break;
        }
        if (read_a != read_b) {
            *equal_out = false;
            break;
        }
        if (read_a == 0) {
            break;
        }
        if (memcmp(buf_a, buf_b, (size_t)read_a) != 0) {
            *equal_out = false;
            break;
        }
    }

    free(buf_a);
    free(buf_b);
    close(fd_a);
    close(fd_b);
    return rc;
}

bool vd_is_likely_text_file(const char *path) {
    int fd = open(path, O_RDONLY);
    if (fd < 0) {
        return false;
    }

    unsigned char buffer[8192];
    ssize_t nread = read(fd, buffer, sizeof(buffer));
    close(fd);
    if (nread < 0) {
        return false;
    }
    if (nread == 0) {
        return true;
    }

    for (ssize_t i = 0; i < nread; ++i) {
        unsigned char ch = buffer[i];
        if (ch == '\0') {
            return false;
        }
        if ((ch < 9 || ch > 126) && ch != '\n' && ch != '\r' && ch != '\t' && ch != '\f') {
            return false;
        }
    }
    return true;
}
