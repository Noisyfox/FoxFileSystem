#ifndef __FILE_H_FFS
#define __FILE_H_FFS

#include "f_types.h"
#include "directory.h"
#include "virtual_file.h"

class File
{
private:
    friend class Directory;

    VFile* vfile_service;
    vfile_t* vfile;

    bool r, w, a;

    File(VFile* vfile_s, vfile_t* file, bool read, bool write, bool append);
public:
    bool Close();

    __int64 Truncate(file_size_t size);
    offset_t Seek(offset_t offset, __int32 origin);
    offset_t Tell();
    size_t Write(void const* buffer, size_t size);
    size_t Read(void* buffer, size_t size);
};

#endif
