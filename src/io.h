#pragma once

#include "types.h"
#include <stdbool.h>

/* data is NULL, if it failed to map. */
typedef struct FileView {
    u64 length;
    byte* data;

#if defined(ZULK_WIN32)
    /* for platform specific stuff, not for user data! */
    void* extra;
#endif
} FileView;

FileView file_view_open(const char* path);
void file_view_close(FileView* view);
