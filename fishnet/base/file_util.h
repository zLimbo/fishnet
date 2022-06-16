#pragma once

#include "fishnet/base/noncopyable.h"
#include "fishnet/base/string_piece.h"
#include <sys/types.h>

namespace fishnet {

namespace file_util {

// 读小文件 < 64KB
class ReadSmallFile : noncopyable {
public:
    ReadSmallFile(StringArg filename);
    ~ReadSmallFile();

    template <typename String>
    int readToString(int maxSize, String* content, int64_t* fileSize,
                     int64_t* modifyTime, int64_t* createTime);

    int readToBuffer(int* size);

    const char* buffer() const {
        return buf_;
    }

    static const int kBufferSize = 64 * 1024;

private:
    int fd_;
    int err_;
    char buf_[kBufferSize];
};

template <typename String>
int readFile(StringArg filename, int maxSize, String* content,
             int64_t* fileSize = NULL, int64_t* modifyTime = NULL,
             int64_t* createTime = NULL) {
    ReadSmallFile file(filename);
    return file.readToString(maxSize, content, fileSize, modifyTime,
                             createTime);
}

class AppendFile : noncopyable {
public:
    explicit AppendFile(StringArg filename);

    ~AppendFile();

    void append(const char* logline, size_t len);

    void flush();

    off_t writtenBytes() const {
        return writtenBytes_;
    }

private:
    size_t write(const char* logline, size_t len);

    FILE* fp_;
    char buffer_[64 * 1024];
    off_t writtenBytes_;
};

}  // namespace file_util
}  // namespace fishnet