#include "io.h"

#if defined(ZULK_WIN32)
#define WIN32_LEAN_AND_MEAN 1
#include <Windows.h>

struct WindowsFileViewInternal {
    HANDLE file;
    HANDLE view;
};

FileView file_view_open(const char* path) {
    FileView view;
    HANDLE file = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_READONLY, NULL);
    if (file == INVALID_HANDLE_VALUE) {
        view.data = NULL;
        return view;
    }

    view.length = GetFileSize(file, ((unsigned long*)(char*)(&view.length) + sizeof(u32)));

    HANDLE file_mapping = CreateFileMappingA(file, NULL, PAGE_READONLY, 0, 0, NULL);
    if (file_mapping == NULL) {
        view.data = NULL;
        return view;
    }

    view.data = MapViewOfFile(file_mapping, FILE_MAP_READ, 0, 0, 0);
    struct WindowsFileViewInternal* internal = view.extra = VirtualAlloc(NULL, sizeof(struct WindowsFileViewInternal), MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);

    internal->file = file;
    internal->view = file_mapping;

    return view;
}

void file_view_close(FileView* view) {
    struct WindowsFileViewInternal* internal = view->extra;

    UnmapViewOfFile(view->data);
    CloseHandle(internal->view);
    CloseHandle(internal->file);

    VirtualFree(internal, 0, MEM_RELEASE);
}
#else 
#include <sys/stat.h>
#include <unistd.h>

FileView file_view_open(const char* path) {
    FileView view;

    int fd = open(file_name.data(), O_RDONLY);
    if (fd == -1) {
        view.data = NULL;
        return view;
    }

    struct stat file;
    if (fstat(fd, &file) == -1) {
        view.data = NULL;
        close(fd);
        return view;
    }

    view.length = file.st_size;
    view.data = mmap(nullptr, length, PROT_READ, MAP_PRIVATE, fd, 0);
    if (view.data == MAP_FAILED) {
        view.data = NULL;
        close(fd);
        return view.data;
    }

    close(fd);
}
void file_view_close(FileView* view) {
    munmap(view.data, view.length);
}
#endif
