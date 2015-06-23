#ifndef __CLUSTER_H_FFS
#define __CLUSTER_H_FFS

#include <cstdio>

#include "lru.hpp"
#include "SparseArray.h"

typedef unsigned __int32 cluster_t;
// End Of Cluster
#define EOC ((cluster_t)0xffffffff)
#define MAX_STACK 512

#define CLUSTER_4K 4096

// 保留簇定义，这些簇是不能被释放掉的，用来做特定用途
// 这些保留簇拥有确定的簇序号，并且在创建完分区后就可以访问
// 由于本系统设计为动态分配到的簇的序号事先不可知，为了能够
// 让上层应用能够从确定的位置读取到第一块的数据，故保留了3个簇。
// 其中 MM 代表 MMC，是簇管理系统用来存放簇分配信息的，上层应用
// 不应该访问本簇。
// PRIMARY 和 SECONDARY 可自由使用。设置两个空闲保留簇的目的是
// 便于进行冗余设计。
#define CLUSTER_REV_MM 0
#define CLUSTER_REV_PRIMARY 1
#define CLUSTER_REV_SECONDARY 2

#define CLUSTER_REV_MAX CLUSTER_REV_SECONDARY
#define CLUSTER_REV_COUNT (CLUSTER_REV_MAX + 1)

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
    void NewRef();
    bool ReleaseRef(); // 返回是否 free

    bool Modify();
public:
    ~ClusterContainer();
    bool Sync();
    bool Avaliable();
    size_t Read(size_t dst_offset, size_t src_offset, size_t count, unsigned __int8* dst);
    size_t Write(size_t dst_offset, size_t src_offset, size_t count, unsigned __int8* src);
    size_t Memset(size_t dst_offset, size_t count, unsigned __int8 value);

    cluster_t GetCluster();
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

    bool IsActive(cluster_t id);
};

#endif
