#ifndef __CLUSTER_H
#define __CLUSTER_H

#include <cstdio>

#include "lru.hpp"
#include "SparseArray.h"

typedef unsigned __int32 cluster_t;
// End Of Cluster
#define EOC ((cluster_t)0xffffffff)
#define MAX_STACK 512

#define CLUSTER_4K 4096

typedef struct
{
    size_t cluster_size; // 簇大小
    cluster_t cluster_count; // 簇数量
    cluster_t free_cluster_count; // 空闲簇数量
} ClusterInfo;

typedef struct
{
    cluster_t count;
    cluster_t next_stack;
    cluster_t stack[MAX_STACK];
} FreeStack;

#define MASK_FALG 0xF000
#define MASK_DCOUNT 0x0FFF // dirty 计数

#define FLAG_AVAILABLE 0x8000
#define FLAG_DIRTY 0x1000

#define DIRTY_MAX 1024

class ClusterContainer
{
private:
    friend class ClusterMgr;

    ClusterMgr* cluster_mgr;
    FILE* fd;
    ClusterInfo* info;

    cluster_t cluster;
    unsigned __int8* buffer;
    unsigned __int16 flag;
    unsigned __int32 ref;
    
    ClusterContainer(ClusterMgr* mgr, cluster_t cluster);
    ~ClusterContainer();
    bool Sync();
    void NewRef();
    bool ReleaseRef(); // 返回是否 free
public:
    bool Avaliable();
    size_t Read(size_t dst_offset, size_t src_offset, size_t count, unsigned __int8* dst);
    size_t Write(size_t dst_offset, size_t src_offset, size_t count, unsigned __int8* src);
};

class ClusterMgr
{
private:
    friend class ClusterContainer;

    bool opened;
    FILE* fd;
    ClusterInfo info;
    ClusterContainer* MMC; // Master Meta Cluster 主元数据簇

    LruCache<cluster_t, ClusterContainer*>* inactive_cache; // 非活动缓存，LRU
    SparseArray<cluster_t, ClusterContainer*>* active_cache; // 活动缓存

    ClusterContainer* _Fetch(cluster_t cluster);
public:
    ClusterMgr();
    ~ClusterMgr();

    static bool CreatePartition(const char* file, ClusterInfo* info); // 创建分区文件
    bool LoadPartition(const char* file); // 载入分区文件
    bool ClosePartition(); // 关闭分区文件

    size_t GetClusterSize(); // 获取簇大小
    cluster_t GetFreeCluster(); // 获取空闲簇数量

    cluster_t* Allocate(cluster_t count, cluster_t* out); // 分配 count 个簇，储存在out中，如果成功，返回out，否则返回null
    bool Free(cluster_t cluster); // 释放一个 cluster

    ClusterContainer* Fetch(cluster_t cluster); // 将序号为 cluster 的簇的数据加载到内存中
    bool Dispose(ClusterContainer& cluster); // 释放 cluster 中引用的数据，此时可以将内存中的数据写回到磁盘

    bool Sync(); // 将所有挂起的修改全部写入磁盘
};

#endif
