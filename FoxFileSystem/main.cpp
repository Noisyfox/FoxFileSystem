
#include "cluster.h"
#include "node.h"

int main()
{
    ClusterInfo info = {
        CLUSTER_4K, 16
    };
    bool ret = ClusterMgr::CreatePartition("Z:\\part0.f", &info);
    ClusterMgr cluster_mgr;
    if(!ret)
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

    ret = root_node->Truncate(32 * 1024);
    if (!ret)
    {
        return -1;
    }
    for (size_t i = 0; i < 32 * 1024; i++)
    {
        ret |= root_node->Write("a", 1) == 1;
    }
    if (!ret)
    {
        return -1;
    }
    ret = root_node->Seek(31, SEEK_SET) == 31;
    if (!ret)
    {
        return -1;
    }
    ret = root_node->Truncate(32);
    if (!ret)
    {
        return -1;
    }
    ret = node_mgr.Close(root_node);
    if (!ret)
    {
        return -1;
    }

    ret = cluster_mgr.ClosePartition();

    return 0;
}