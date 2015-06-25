#include "file.h"

// TODO: 权限检查，如只读，只写

File::File(VFile* vfile_s, vfile_t* file, bool read, bool write, bool append) :
    vfile_service(vfile_s),
    vfile(file),
    r(read),
    w(write),
    a(append)
{
    if(a)
    {
        Seek(0, SEEK_END);
    }
}

bool File::Close()
{
    return vfile_service->Close(vfile);
}

__int64 File::Truncate(file_size_t size)
{
    if(!w)
    {
        return EOF;
    }

    return vfile_service->Truncate(vfile, size);
}

offset_t File::Seek(offset_t offset, __int32 origin)
{
    return vfile_service->Seek(vfile, offset, origin);
}

offset_t File::Tell()
{
    return vfile_service->Tell(vfile);
}

size_t File::Write(void const* buffer, size_t size)
{
    if (!w)
    {
        return 0;
    }

    return vfile_service->Write(vfile, buffer, size);
}

size_t File::Read(void* buffer, size_t size)
{
    if (!r)
    {
        return 0;
    }

    return vfile_service->Read(vfile, buffer, size);
}
