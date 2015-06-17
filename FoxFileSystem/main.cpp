
#include "cluster.h"

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
    ret = cluster_mgr.ClosePartition();

    return 0;
}