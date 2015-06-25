
#include "cluster.h"
#include "node.h"
#include "virtual_file.h"
#include "directory.h"
#include "shell.h"

int main()
{
    // ÊäÈë´ÅÅÌÎÄ¼þ
    char path[MAX_PATH];
    cluster_t cluster_count = 0;
    printf("Input partition file location: ");
    scanf("%s", path);
    printf("Input partition cluster count: ");
    scanf("%ud", &cluster_count);
    getchar();

    ClusterInfo info = {
        CLUSTER_4K, cluster_count
    };
    bool ret = ClusterMgr::CreatePartition(path, &info);
    ClusterMgr* cluster_mgr = new ClusterMgr();
    if (!ret)
    {
        fprintf(stderr, "Error create partition!");
        return -1;
    }
    ret = cluster_mgr->LoadPartition(path);
    if (!ret)
    {
        fprintf(stderr, "Error load partition!");
        return -1;
    }

    NodeMgr* node_mgr = new NodeMgr(cluster_mgr);
    Node* root_node = node_mgr->CreateRootNode();
    ret = node_mgr->Close(root_node);
    if (!ret)
    {
        fprintf(stderr, "Error create root node!");
        return -1;
    }

    VFile* vfile = new VFile(node_mgr);

    ret = Directory::CreateRootDirectory(vfile);
    if (!ret)
    {
        fprintf(stderr, "Error create root dir!");
        return -1;
    }

    Directory* directory = new Directory(vfile);
    ret = directory->Init();
    if (!ret)
    {
        fprintf(stderr, "Error reading dir!");
        return -1;
    }

    /*
    directory->MkDir("xyz");
    directory->MkDir("xyz/sdd");
    directory->MkDir("xyz/sdd/baa");

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

    dir = directory->OpenDirectory("xyz/../xyz/./sdd/baa");
    char path[MAX_PATH];
    dir->GetAbsolutePath(path);
    printf("%s\n", path);
    directory->CloseDirectory(dir);

    directory->GetWD(path, MAX_PATH);
    printf("%s\n", path);


    */

    shell_main(directory);

    delete directory;
    delete vfile;
    delete node_mgr;

    ret = cluster_mgr->ClosePartition();

    delete cluster_mgr;

    return 0;
}