#ifndef COPY_TREE_H
#define COPY_TREE_H
#include "copy_file.h"
#include <stdint.h>
#include <stddef.h>
typedef struct {
    int recursive, skip_existing, verify, quiet, dry_run, preserve;
    volatile sig_atomic_t *cancelled;
    void (*progress)(off_t, off_t, void *);
    void *user;
    const char *excludes[16];
    size_t exclude_count;
    uintmax_t files, folders, links, skipped, errors, bytes, excluded;
} CopyJob;
/* Exact target semantics: directory contents go directly into dest. */
int copy_path(const char *src, const char *dest, CopyJob *job);
#endif
