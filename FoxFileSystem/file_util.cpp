#include "file_util.h"
#include "util.h"

bool FileCut(VFile* vfile, vfile_t* vf, offset_t from, size_t len)
{
    if(len == 0)
    {
        return true;
    }

    if(from < 0)
    {
        return false;
    }

    offset_t read_p = from + len, write_p = from;
    size_t read_size;
    byte_t d[1024];

    while(1)
    {
        ASSERT_SIZE(vfile->Seek(vf, read_p, SEEK_SET), read_p);
        read_size = vfile->Read(vf, d, sizeof(d));
        if(read_size == 0)
        {
            break;
        }
        ASSERT_SIZE(vfile->Seek(vf, write_p, SEEK_SET), write_p);
        ASSERT_SIZE(vfile->Write(vf, d, read_size), read_size);
        read_p += read_size;
        write_p += read_size;
    }

    // ËõÐ¡ÎÄ¼þ
    ASSERT_EOF(vfile->Truncate(vf, write_p));

    return true;
faild:
    return false;
}

int CloseFile(File* f)
{
    f->Close();
    delete f;

    return 0;
}
