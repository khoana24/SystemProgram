#ifndef MYCP_H
#define MYCP_H

#include <sys/stat.h>

int copy_path(const char *src, const char *dest);
int copy_file_data(const char *src, const char *dest);
int copy_directory(const char *src, const char *dest);
int apply_metadata(const char *src, const char *dest,
                   const struct stat *src_stat, int is_symlink);

#endif
