#include "directory.h"
#include "util.h"
#include "file_util.h"

#define IS_DIR(_node) ((_node)->node->GetMode(MODE_MASK_TYPE) == TYPE_DIR)
#define IS_FILE(_node) ((_node)->node->GetMode(MODE_MASK_TYPE) == TYPE_NORMAL)

#define CLOSE_DIR(_dir) do{if(_dir != NULL){_dir->Close(); delete _dir; _dir = NULL;}}while(0)

inline size_t Padding4(size_t orig)
{
    size_t total_len = orig;
    size_t d = orig & 0x3;
    if (d != 0)
    {
        total_len += 4 - d;
    }

    return total_len;
}

cluster_t DirectoryFile::NextEntry(bool start)
{
    if (start)
    {
        entry_next_offset = 0;
    }
    ASSERT_SIZE(vfile_service->Seek(dir_vfile, entry_next_offset, SEEK_SET), entry_next_offset);

    byte_t name_length = 0;
    cluster_t node_id = EOC;
    char name[MAX_FILE];
    memset(name, 0, sizeof(name));

    entry_current_offset = entry_next_offset;

    // 先读入文件名长度
    ASSERT_SIZE(vfile_service->Read(dir_vfile, &name_length, sizeof(byte_t)), sizeof(byte_t));
    if (name_length == 0) {
        // 当长度为0代表结束
        entry_next_offset = vfile_service->Tell(dir_vfile);
        entry_current_size = entry_next_offset - entry_current_offset;
        goto faild;
    }

    // 按4字节对齐计算文件名实际长度
    size_t total_len = Padding4(name_length);
    // 读入文件名
    ASSERT_SIZE(vfile_service->Read(dir_vfile, name, total_len), total_len);

    // 读入node id
    ASSERT_SIZE(vfile_service->Read(dir_vfile, &node_id, sizeof(cluster_t)), sizeof(cluster_t));

    entry_next_offset = vfile_service->Tell(dir_vfile);
    entry_current_size = entry_next_offset - entry_current_offset;
    strcpy(entry_current, name);

    return node_id;
faild:
    return EOC;
}

cluster_t DirectoryFile::FindEntry(char const* n)
{
    entry_offset = EOF;
    entry_size = 0;

    size_t t_length = strlen(n);

    if (t_length >= MAX_FILE)
    {
        goto faild;
    }

    byte_t name_length = 0;
    cluster_t node_id = EOC;
    char name[MAX_FILE];
    memset(name, 0, sizeof(name));

    ASSERT_SIZE(vfile_service->Seek(dir_vfile, 0, SEEK_SET), 0);

    offset_t curr_start_offset = 0;
    while (1)
    {
        curr_start_offset = vfile_service->Tell(dir_vfile);
        // 先读入文件名长度
        ASSERT_SIZE(vfile_service->Read(dir_vfile, &name_length, sizeof(byte_t)), sizeof(byte_t));
        if (name_length == 0) {
            // 当长度为0代表结束
            entry_offset = curr_start_offset;
            entry_size = vfile_service->Tell(dir_vfile) - curr_start_offset;
            break;
        }

        // 按4字节对齐计算文件名实际长度
        size_t total_len = Padding4(name_length);
        // 读入文件名
        ASSERT_SIZE(vfile_service->Read(dir_vfile, name, total_len), total_len);

        // 读入node id
        ASSERT_SIZE(vfile_service->Read(dir_vfile, &node_id, sizeof(cluster_t)), sizeof(cluster_t));

        // 比较文件名
        if (name_length == t_length)
        {
            if (strncmp(n, name, name_length) == 0)
            {
                entry_offset = curr_start_offset;
                entry_size = vfile_service->Tell(dir_vfile) - curr_start_offset;
                return node_id;
            }
        }
    }
faild:
    return EOC;
}

DirectoryFile::DirectoryFile(VFile* vfile_srv, vfile_t* vfile) :
    vfile_service(vfile_srv),
    dir_vfile(vfile)
{
}

vfile_t* DirectoryFile::Open(char const* n)
{
    cluster_t node_id = FindEntry(n);
    ASSERT_EOC(node_id);

    // 得到了node id
    return vfile_service->Open(node_id);
faild:
    return NULL;
}

bool DirectoryFile::Close()
{
    return vfile_service->Close(dir_vfile);
}

vfile_t* DirectoryFile::DuplicateFile()
{
    return vfile_service->Open(dir_vfile->node->GetNodeId());
}

DirectoryFile* DirectoryFile::Duplicate()
{
    vfile_t* f = DuplicateFile();

    ASSERT_NULL(f);

    return new DirectoryFile(vfile_service, f);
faild:
    return NULL;
}

bool DirectoryFile::AddFile(char const* name, cluster_t node)
{
    size_t t_length = strlen(name);

    if (t_length >= MAX_FILE)
    {
        goto faild;
    }

    if (FindEntry(name) != EOC)
    {
        goto faild;
    }
    offset_t last_entry_offset = entry_offset;
    if (last_entry_offset == EOF)
    {
        last_entry_offset = 0;
    }

    char pt[MAX_PATH];
    memset(pt, 0, sizeof(pt));
    strcpy(pt, name);

    byte_t name_len = (byte_t)t_length;
    size_t name_total_len = Padding4(t_length);

    // 先扩充文件内容并用0填充
    ASSERT_SIZE(vfile_service->Seek(dir_vfile, last_entry_offset, SEEK_SET), last_entry_offset);
    __int64 entry_size = sizeof(byte_t) + name_total_len * sizeof(char) + sizeof(cluster_t);
    while (entry_size >= 0)
    {
        ASSERT_SIZE(vfile_service->Write(dir_vfile, "\0", sizeof(char)), sizeof(char));
        entry_size -= sizeof(char);
    }

    // 再重新写入目录数据
    ASSERT_SIZE(vfile_service->Seek(dir_vfile, last_entry_offset, SEEK_SET), last_entry_offset);
    ASSERT_SIZE(vfile_service->Write(dir_vfile, &name_len, sizeof(byte_t)), sizeof(byte_t));
    ASSERT_SIZE(vfile_service->Write(dir_vfile, pt, sizeof(char) * name_total_len), sizeof(char) * name_total_len);
    ASSERT_SIZE(vfile_service->Write(dir_vfile, &node, sizeof(cluster_t)), sizeof(cluster_t));

    return true;
faild:
    return false;
}

bool DirectoryFile::ReplaceFile(char const* name, cluster_t node)
{
    ASSERT_EOC(FindEntry(name));

    offset_t off = entry_offset;
    if (off == EOF)
    {
        off = 0;
    }
    offset_t node_offset = off + entry_size - sizeof(cluster_t);

    ASSERT_SIZE(vfile_service->Seek(dir_vfile, node_offset, SEEK_SET), node_offset);
    ASSERT_SIZE(vfile_service->Write(dir_vfile, &node, sizeof(cluster_t)), sizeof(cluster_t));

    return true;
faild:
    return false;
}

bool DirectoryFile::RemoveFile(char const* name)
{
    if (strcmp(".", name) == 0 ||
        strcmp("..", name) == 0)
    {
        goto faild;
    }

    ASSERT_EOC(FindEntry(name));

    offset_t off = entry_offset;
    if (off == EOF)
    {
        off = 0;
    }
    return FileCut(vfile_service, dir_vfile, off, entry_size);

faild:
    return false;
}

char* DirectoryFile::GetFileName(cluster_t node, char* name)
{
    cluster_t c_node;

    if ((c_node = NextEntry(true)) != EOC)
    {
        do
        {
            if (c_node == node)
            {
                strcpy(name, entry_current);
                return name;
            }
        } while ((c_node = NextEntry(false)) != EOC);
    }

    return NULL;
}

char* DirectoryFile::GetAbsolutePath(char* path)
{
    DirectoryFile* p[MAX_PATH];
    size_t count = 0;
    char* path_bak = path;

    DirectoryFile* dir = Duplicate();
    
    while(1)
    {
        p[count++] = dir;
        if(dir->IsRoot())
        {
            break;
        }
        dir = dir->OpenParentDirectory();
        ASSERT_NULL(dir);
    }

    char n[MAX_FILE];
    for (int i = count - 2; i >= 0; i--)
    {
        DirectoryFile* c = p[i];
        if(!c->IsRoot())
        {
            DirectoryFile* parent = p[i + 1];
            ASSERT_NULL(parent->GetFileName(c->dir_vfile->node->GetNodeId(), n));
            path += sprintf(path, "/%s", n);
        }
    }

    for (int i = 0; i < count; i++)
    {
        p[i]->Close();
        delete p[i];
    }

    return path_bak;
faild:
    for (int i = 0; i < count; i++)
    {
        p[i]->Close();
        delete p[i];
    }
    return NULL;
}

bool DirectoryFile::IsEmpty()
{
    if (NextEntry(true) != EOF)
    {
        do
        {
            if (strcmp(".", entry_current) != 0 &&
                strcmp("..", entry_current) != 0)
            {
                return false;
            }
        } while (NextEntry(false) != EOF);
    }

    return true;
}

bool DirectoryFile::IsRoot()
{
    cluster_t p = FindEntry(".");
    cluster_t c = FindEntry("..");

    return p == c && p != EOC;
}

DirectoryFile* DirectoryFile::OpenParentDirectory()
{
    vfile_t* f;
    f = Open("..");
    ASSERT_NULL(f);

    return new DirectoryFile(vfile_service, f);
faild:
    return NULL;
}

vfile_t* Directory::Open(char const* path)
{
    char pt[MAX_PATH];

    if (strlen(path) >= MAX_PATH)
    {
        return NULL;
    }

    strcpy(pt, path);

    vfile_t* target_file = NULL;
    DirectoryFile* df = NULL;

    if (path[0] == '/')
    {
        target_file = root_file->DuplicateFile();
    }
    else
    {
        target_file = cwd_file->DuplicateFile();
    }

    char* p;
    p = strtok(pt, "/");
    if (p != NULL)
    {
        do
        {
            ASSERT_FALSE(IS_DIR(target_file));
            df = new DirectoryFile(vfile_service, target_file); // 打开当前目录

            target_file = df->Open(p); // 打开下一级文件
            ASSERT_NULL(target_file);

            DirectoryFile* tmp = df; // 关闭当前目录
            df = NULL;
            bool ret = tmp->Close();
            delete tmp;
            ASSERT_FALSE(ret);
        } while ((p = strtok(NULL, "/")) != NULL);
    }

    return target_file;
faild:
    if (target_file != NULL)
    {
        vfile_service->Close(target_file);
    }
    CLOSE_DIR(df);
    return NULL;
}

DirectoryFile* Directory::OpenParentDirectory(char const* path)
{
    char pt[MAX_PATH];

    if (strlen(path) >= MAX_PATH)
    {
        return NULL;
    }

    strcpy(pt, path);

    char* p = strrchr(pt, '/');

    if (p == NULL)
    {
        return cwd_file->Duplicate();
    }
    else if (p == pt)
    {
        return root_file->Duplicate();
    }
    else
    {
        *p = '\0';
        return OpenDirectory(pt);
    }
}

Directory::Directory(VFile* vfile) :
    vfile_service(vfile),
    cwd_file(NULL),
    root_file(NULL)
{
    memset(cwd, 0, sizeof(cwd));
}

Directory::~Directory()
{
    CLOSE_DIR(cwd_file);
    CLOSE_DIR(root_file);
}

bool Directory::CreateDirectory(VFile* vfile, vfile_t* vf, cluster_t parent)
{
    DirectoryFile* rd = NULL;
    vfile_t* root = vf;

    root->node->SetMode(TYPE_DIR, MODE_MASK_TYPE);

    rd = new DirectoryFile(vfile, root);

    ASSERT_FALSE(rd->AddFile(".", vf->node->GetNodeId()));
    ASSERT_FALSE(rd->AddFile("..", parent));

    CLOSE_DIR(rd);

    return true;
faild:
    if (rd != NULL)
    {
        CLOSE_DIR(rd);
    }
    else if (root != NULL)
    {
        vfile->Close(root);
    }
    return false;
}

bool Directory::RemoveDirectory(DirectoryFile* dir)
{
    vfile_service->Delete(dir->dir_vfile);
    delete dir;
    return true;
}

vfile_t* Directory::CreateFile(char const* path)
{
    DirectoryFile* parent = OpenParentDirectory(path);
    char file_name[MAX_FILE];
    vfile_t* dir = NULL;

    ASSERT_NULL(parent); // 父目录不存在

    memset(file_name, 0, sizeof(file_name));

    const char* p = strrchr(path, '/');
    if (p == NULL)
    {
        strcpy(file_name, path);
    }
    else
    {
        strcpy(file_name, p + 1);
    }

    if (file_name[0] == '\0')
    {
        goto faild;
    }

    if (strcmp(".", file_name) == 0 ||
        strcmp("..", file_name) == 0)
    {
        goto faild;
    }

    dir = vfile_service->Create();
    ASSERT_NULL(dir);

    cluster_t id = dir->node->GetNodeId();

    ASSERT_FALSE(parent->AddFile(file_name, id));

    CLOSE_DIR(parent);

    return dir;
faild:
    CLOSE_DIR(parent);
    if (dir != NULL)
    {
        vfile_service->Delete(dir);
    }
    return NULL;
}

bool Directory::CreateRootDirectory(VFile* vfile)
{
    vfile_t* root = NULL;

    root = vfile->Open(ROOT_DIRECTORY);
    ASSERT_NULL(root);

    return CreateDirectory(vfile, root, ROOT_DIRECTORY);
faild:
    return false;
}

DirectoryFile* Directory::OpenDirectory(char const* path)
{
    vfile_t* target = Open(path);
    ASSERT_NULL(target);

    ASSERT_FALSE(IS_DIR(target));

    DirectoryFile* df;
    df = new DirectoryFile(vfile_service, target);
    ASSERT_NULL(df);

    return df;
faild:
    if (target != NULL)
    {
        vfile_service->Close(target);
    }
    return NULL;
}

int Directory::CloseDirectory(DirectoryFile* dir)
{
    dir->Close();
    delete dir;

    return 0;
}

bool Directory::Init()
{
    vfile_t* root = NULL;
    DirectoryFile* rd = NULL;

    root = vfile_service->Open(ROOT_DIRECTORY);
    ASSERT_NULL(root);

    ASSERT_FALSE(IS_DIR(root));

    rd = new DirectoryFile(vfile_service, root);

    root_file = rd;

    ChDir("/");

    return true;
faild:
    if (rd != NULL)
    {
        CLOSE_DIR(rd);
    }
    else if (root != NULL)
    {
        vfile_service->Close(root);
    }
    return false;
}

char* Directory::GetWD(char* buf, size_t size)
{
    size_t l = strlen(cwd);
    if (buf == NULL)
    {
        size = l + 1;
        buf = new char[l + 1];
    }
    if (l >= size)
    {
        return NULL;
    }

    strncpy(buf, cwd, size);

    return buf;
}

int Directory::ChDir(char const* path)
{
    DirectoryFile* target = OpenDirectory(path);

    if (target == NULL)
    {
        return EOF;
    }

    CLOSE_DIR(cwd_file);

    char cwd_t[MAX_PATH];
    ASSERT_NULL(target->GetAbsolutePath(cwd_t));
    cwd_file = target;
    strncpy(cwd, cwd_t, MAX_PATH);

    return 0;
faild:
    CLOSE_DIR(target);
    return EOF;
}

int Directory::MkDir(char const* path)
{
    DirectoryFile* parent = OpenParentDirectory(path);
    char file_name[MAX_FILE];
    vfile_t* dir = NULL;

    ASSERT_NULL(parent); // 父目录不存在

    memset(file_name, 0, sizeof(file_name));

    const char* p = strrchr(path, '/');
    if (p == NULL)
    {
        strcpy(file_name, path);
    }
    else
    {
        strcpy(file_name, p + 1);
    }

    if (file_name[0] == '\0')
    {
        goto faild;
    }

    if (strcmp(".", file_name) == 0 ||
        strcmp("..", file_name) == 0)
    {
        goto faild;
    }

    dir = vfile_service->Create();
    ASSERT_NULL(dir);

    cluster_t id = dir->node->GetNodeId();
    ASSERT_FALSE(CreateDirectory(vfile_service, dir, parent->dir_vfile->node->GetNodeId()));

    ASSERT_FALSE(parent->AddFile(file_name, id));

    CLOSE_DIR(parent);

    return 0;
faild:
    CLOSE_DIR(parent);
    if (dir != NULL)
    {
        vfile_service->Delete(dir);
    }
    return EOF;
}

int Directory::RmDir(char const* path)
{
    DirectoryFile* curr = OpenDirectory(path);
    DirectoryFile* parent = NULL;

    if (curr == NULL) // 当前目录不存在
    {
        goto faild;
    }

    if (curr->IsRoot())
    {
        goto faild;
    }

    ASSERT_FALSE(curr->IsEmpty());

    parent = curr->OpenParentDirectory();
    ASSERT_NULL(parent);

    char name[MAX_FILE];
    ASSERT_NULL(parent->GetFileName(curr->dir_vfile->node->GetNodeId(), name));
    ASSERT_FALSE(parent->RemoveFile(name));
    CLOSE_DIR(parent);

    ASSERT_FALSE(RemoveDirectory(curr));

    return 0;
faild:
    CLOSE_DIR(curr);
    CLOSE_DIR(parent);
    return EOF;
}

int Directory::Remove(char const* path)
{
    DirectoryFile* parent = NULL;
    vfile_t* file = Open(path);
    ASSERT_NULL(file);
    ASSERT_FALSE(IS_FILE(file));

    parent = OpenParentDirectory(path);
    ASSERT_NULL(parent);

    char name[MAX_FILE];
    ASSERT_NULL(parent->GetFileName(file->node->GetNodeId(), name));
    ASSERT_FALSE(parent->RemoveFile(name));
    CLOSE_DIR(parent);

    vfile_service->Delete(file);

    return 0;
faild:
    if (file != NULL)
    {
        vfile_service->Close(file);
    }
    CLOSE_DIR(parent);
    return EOF;
}

File* Directory::OpenFile(char const* path, bool read, bool write, bool create, bool append, bool clear)
{
    vfile_t* file = NULL;

    // 检查状态合法性
    if (!write)
    {
        if (create || append || clear)
        {
            goto faild;
        }
    }
    if (append && clear)
    {
        goto faild;
    }

    file = Open(path);
    if (file != NULL)
    {
        // 检查是不是目录
        ASSERT_FALSE(IS_FILE(file));
    }
    else
    {
        if (!create)
        {
            goto faild;
        }
        // 创建文件
        file = CreateFile(path);
        ASSERT_NULL(file);
    }

    if (clear)
    {
        ASSERT_EOF(vfile_service->Truncate(file, 0));
    }

    return new File(vfile_service, file, read, write, append);

faild:
    if (file != NULL)
    {
        vfile_service->Close(file);
    }
    return NULL;
}

void a()
{

}
