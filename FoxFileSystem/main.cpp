
#include "cluster.h"
#include "node.h"
#include "virtual_file.h"

int main()
{
    ClusterInfo info = {
        CLUSTER_4K, 4096
    };
    bool ret = ClusterMgr::CreatePartition("Z:\\part0.f", &info);
    ClusterMgr cluster_mgr;
    if (!ret)
    {
        return -1;
    }
    ret = cluster_mgr.LoadPartition("Z:\\part0.f");
    if (!ret)
    {
        return -1;
    }

    NodeMgr node_mgr(&cluster_mgr);
    Node* root_node = node_mgr.CreateRootNode();
    ret = node_mgr.Close(root_node);
    if (!ret)
    {
        return -1;
    }

    VFile vfile(&node_mgr);

    vfile_t* vf = vfile.Open(CLUSTER_REV_SECONDARY);
    if (vf == NULL)
    {
        return -1;
    }

    long i = 0;
    unsigned char v = 0, v2 = 0;
    while (1)
    {
        if (vfile.Write(vf, &v, 1) != 1)
        {
            break;
        }
        i++;
        v++;
    }

    if (vfile.Seek(vf, 0, SEEK_SET) != 0)
    {
        return 0;
    }
    i = 0;
    v = 0;
    while (1)
    {
        if (vfile.Read(vf, &v2, 1) != 1)
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

    ret = vfile.Truncate(vf, 0);

    if (vf == NULL)
    {
        return -1;
    }

    ret = cluster_mgr.ClosePartition();

    return 0;
}