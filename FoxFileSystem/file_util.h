#ifndef __FILE_UTIL_H_FFS
#define __FILE_UTIL_H_FFS

#include "virtual_file.h"
#include "file.h"

bool FileCut(VFile* vfile, vfile_t* vf, offset_t from, size_t len);

int CloseFile(File* f);

#endif
