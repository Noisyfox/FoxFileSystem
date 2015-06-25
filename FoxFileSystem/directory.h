#ifndef __DIRECTORY_H_FFS
#define __DIRECTORY_H_FFS

#include "file.h"
#include "virtual_file.h"

#define MAX_PATH 256
#define MAX_FILE 256

#define ROOT_DIRECTORY CLUSTER_REV_SECONDARY

class DirectoryFile
{
private:
    friend class File;
    friend class Directory;

    VFile* vfile_service;
    vfile_t* dir_vfile;

    offset_t entry_offset; // FindEntry ½á¹û
    size_t entry_size;

    char entry_current[MAX_FILE];
    offset_t entry_current_offset;
    size_t entry_current_size;
    offset_t entry_next_offset;

    cluster_t NextEntry(bool start);

    cluster_t FindEntry(char const* name);
public:
    DirectoryFile(VFile* vfile_srv, vfile_t* vfile);

    vfile_t* Open(char const* name);
    bool Close();

    vfile_t* DuplicateFile();
    DirectoryFile* Duplicate();

    bool AddFile(char const* name, cluster_t node);
    bool ReplaceFile(char const* name, cluster_t node);
    bool RemoveFile(char const* name);

    char* GetFileName(cluster_t node, char* name);

    bool IsEmpty();
    bool IsRoot();

    DirectoryFile* OpenParentDirectory();
};

class Directory
{
private:
    friend class File;
    friend class DirectoryFile;

    VFile* vfile_service;

    char cwd[MAX_PATH];

    DirectoryFile* cwd_file;
    DirectoryFile* root_file;

    vfile_t* Open(char const* path);
    
    DirectoryFile* OpenDirectory(char const* path);
    DirectoryFile* OpenParentDirectory(char const* path);

    static bool CreateDirectory(VFile* vfile, vfile_t* vf, cluster_t parent);

    bool RemoveDirectory(DirectoryFile* dir);

    vfile_t* CreateFile(char const* path);

public:
    explicit Directory(VFile* vfile);
    ~Directory();

    static bool CreateRootDirectory(VFile* vfile);

    bool Init();

    char* GetWD(char* buf, size_t size);
    int ChDir(char const* path);
    int MkDir(char const* path);
    int RmDir(char const* path);

    int Remove(char const* path);
    int Rename(char const* old_name, char const* new_name);

    File* OpenFile(char const* path, bool read, bool write, bool create, bool append, bool clear);
};

#endif
