#ifndef __NODE_H
#define __NODE_H

#include "cluster.h"

#define MAX_DIRECT 12
#define MAX_INDIRECT 3

#define MODE_MASK_TYPE 0xF000
#define MODE_MASK_USER 0xF00
#define MODE_MASK_GROUP 0xF0
#define MODE_MASK_OTHER 0xF

#define TYPE_DIR 0x0000
#define TYPE_NORMAL 0x1000

typedef struct
{
    unsigned __int16 flag;
    unsigned __int16 mode;
    unsigned __int32 time_create;
    unsigned __int32 time_modify;
    unsigned __int32 time_visit;
    unsigned __int32 size;
    cluster_t cluster_count; // 暂时没有用到该字段
    cluster_t index_direct[MAX_DIRECT];
    cluster_t index_indir[MAX_INDIRECT];
    __int32 reserved[11]; // padding to 128 byte
    //unsigned __int8 data[0]; // 文件体积小于 簇尺寸 - 128B 的部分直接放置在这里
} INode;

typedef struct
{
    size_t no_index;
    size_t index_direct[MAX_DIRECT];
    size_t index_indir[MAX_INDIRECT];

    cluster_t index_per_cluster; // 每个簇可储存多少个索引

    size_t size_level[MAX_INDIRECT + 1]; // 各级索引可容纳的尺寸
} CIndex; // 用来记录每一级索引分别可以保存多少字节数据的结构体

class Node
{
private:
    friend class NodeMgr;

    NodeMgr* node_mgr;
    ClusterContainer* MC; // Main Cluster
    INode inode;
    size_t cdo; // current data offset，当前访问指针位置

    size_t seek_index[MAX_INDIRECT + 1]; // ClusterSeek方法结果储存 多级索引路径
    size_t seek_cluster_offset; // ClusterSeek方法结果储存 簇内偏移

    ClusterContainer* target_cluster;
    size_t target_offset;
    size_t target_cluster_offset;

    ClusterContainer* current; // 当前数据簇
    size_t cco; // current cluster offset，当前簇内偏移

    unsigned __int16 flag;
    CIndex index_boundary;

    Node(NodeMgr* mgr, ClusterContainer* mc);
    bool Init();
    bool Load();
    bool Modify();

    __int8 ClusterSeek(size_t offset); // 计算offset对应的偏移量在哪个簇中
    bool _LoadCluster(size_t offset);
    ClusterContainer* LoadClusterByIndex(__int8 depth, size_t* index);
    bool LoadCluster(size_t offset);

    bool Shrink(size_t curr, size_t target);
    bool Expand(size_t curr, size_t target);

    bool RemoveClusterRight(__int8 depth, cluster_t index, size_t* edge_right);
    bool RemoveClusterLeft(__int8 depth, cluster_t index, size_t* edge_left);
    bool RemoveCluster(__int8 depth, cluster_t index, size_t* edge_left, size_t* edge_right);

    cluster_t BuildClusterRight(__int8 depth, size_t* edge_right);
    bool BuildClusterLeft(__int8 depth, cluster_t index, size_t* edge_left);
    bool BuildCluster(__int8 depth, cluster_t index, size_t* edge_left, size_t* edge_right);
public:
    cluster_t GetNodeId();

    bool Sync();

    unsigned __int16 GetMode(unsigned __int16 mask);
    bool SetMode(unsigned __int16 mode, unsigned __int16 mask);

    size_t GetSize();
    bool Truncate(size_t size); // 改变 Node 大小

    size_t GetPointer(); // 获得当前访问指针位置
    __int64 Seek(size_t offset); // 改变当前访问指针
    size_t Write(void const* buffer, size_t size);
    size_t Read(void* buffer, size_t size);
};

class NodeMgr
{
private:
    friend class Node;

    ClusterMgr* cluster_service;

    CIndex index_boundary;

    Node* CreateNode(ClusterContainer* mc);

public:
    NodeMgr(ClusterMgr* cluster_mgr);

    Node* CreateRootNode();
    Node* OpenRootNode();

    Node* CreateNode();
    Node* OpenNode(cluster_t node);
    bool Close(Node* node); // 关闭该Node，写入全部修改到磁盘
    bool Delete(Node* node); // 关闭并删除该Node

    bool Sync();
};

#endif
