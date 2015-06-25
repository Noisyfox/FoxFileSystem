
#include "cluster.h"
#include "node.h"
#include "virtual_file.h"
#include "directory.h"
#include "file.h"
#include "file_util.h"

int main()
{
    ClusterInfo info = {
        CLUSTER_4K, 4096
    };
    bool ret = ClusterMgr::CreatePartition("Z:\\part0.f", &info);
    ClusterMgr* cluster_mgr = new ClusterMgr();
    if (!ret)
    {
        return -1;
    }
    ret = cluster_mgr->LoadPartition("Z:\\part0.f");
    if (!ret)
    {
        return -1;
    }

    NodeMgr* node_mgr = new NodeMgr(cluster_mgr);
    Node* root_node = node_mgr->CreateRootNode();
    ret = node_mgr->Close(root_node);
    if (!ret)
    {
        return -1;
    }

    VFile* vfile = new VFile(node_mgr);

    ret = Directory::CreateRootDirectory(vfile);
    if (!ret)
    {
        return -1;
    }

    Directory* directory = new Directory(vfile);
    ret = directory->Init();
    if (!ret)
    {
        return -1;
    }

    directory->MkDir("xyz");

    File* file = directory->OpenFile("xyz/abcd.h", true, true, true, false, true);
    if (file == NULL)
    {
        return -1;
    }

    long i = 0;
    unsigned char v = 0, v2 = 0;
    while (1)
    {
        if (file->Write(&v, 1) != 1)
        {
            break;
        }
        i++;
        v++;
    }

    if (file->Seek(0, SEEK_SET) != 0)
    {
        return 0;
    }
    i = 0;
    v = 0;
    while (1)
    {
        if (file->Read(&v2, 1) != 1)
        {
            break;
        }
        if (v2 != v)
        {
            return -1;
        }
        i++;
        v++;
    }

    ret = file->Truncate(0) != EOF;

    CloseFile(file);

    DirectoryFile* dir = directory->OpenDirectory(".");
    if(dir->NextEntry(true) != EOC)
    {
        do
        {
            printf("%s\n", dir->entry_current);
        } while (dir->NextEntry(false) != EOC);
    }
    directory->CloseDirectory(dir);

    delete directory;
    delete vfile;
    delete node_mgr;

    ret = cluster_mgr->ClosePartition();

    delete cluster_mgr;

    return 0;
}