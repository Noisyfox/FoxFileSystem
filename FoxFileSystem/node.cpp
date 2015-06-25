#define _USE_32BIT_TIME_T

#include <ctime>

#include "node.h"
#include "util.h"

#define SEEK_EMPTY -2
#define SEEK_FAIL -3

Node::Node(NodeMgr* mgr, ClusterContainer* mc) :
    node_mgr(mgr),
    MC(mc),
    cdo(0),
    flag(0),
    index_boundary(mgr->index_boundary)
{
    memset(&inode, 0, sizeof(INode));

    LoadCluster(0);
}

bool Node::Init()
{
    inode.time_create = inode.time_modify = inode.time_visit = time(NULL);
    int i;
    for (i = 0; i < MAX_DIRECT; i++)
    {
        inode.index_direct[i] = EOC;
    }
    for (i = 0; i < MAX_INDIRECT; i++)
    {
        inode.index_indir[i] = EOC;
    }

    ASSERT_SIZE(MC->Write(0, 0, sizeof(INode), (unsigned __int8*)&inode), sizeof(INode));
    ASSERT_FALSE(Modify());

    return true;
faild:
    return false;
}

bool Node::Load()
{
    ASSERT_SIZE(MC->Read(0, 0, sizeof(INode), (unsigned __int8*)&inode), sizeof(INode));
    inode.time_visit = time(NULL);
    Modify();

    return true;
faild:
    return false;
}

bool Node::Modify()
{
    flag |= FLAG_DIRTY;

    unsigned __int16 dirty_count = flag & MASK_DCOUNT;
    if (dirty_count >= DIRTY_MAX)
    {
        return Sync();
    }

    dirty_count++;
    flag &= ~MASK_DCOUNT;
    flag |= (dirty_count & MASK_DCOUNT);

    return true;
}

__int8 Node::ClusterSeek(size_t offset)
{
    if (offset == -1) // 特殊情况，在计算尺寸边界的时候用
    {
        seek_cluster_offset = sizeof(INode) + offset;
        return -1; // 不需要索引，直接可以储存在 MC 中
    }

    if (offset < 0)
    {
        return SEEK_FAIL; // 出错
    }

    if (offset < index_boundary.no_index)
    {
        seek_cluster_offset = sizeof(INode) + offset;
        return -1; // 不需要索引，直接可以储存在 MC 中
    }
    int i;
    file_size_t size_prev = index_boundary.no_index;
    file_size_t size_curr, size_level_max;
    size_level_max = index_boundary.index_direct[MAX_DIRECT - 1];
    if (offset < size_level_max) {
        for (i = 0; i < MAX_DIRECT; i++) // 直接索引
        {
            size_curr = index_boundary.index_direct[i];
            if (offset < size_curr)
            {
                seek_cluster_offset = (size_t)(offset - size_prev);
                seek_index[0] = i;
                return 0;
            }
            size_prev = size_curr;
        }
    }
    else
    {
        size_prev = size_level_max;
    }

    int j;
    size_level_max = index_boundary.index_indir[MAX_INDIRECT - 1];
    if (offset < size_level_max) {
        for (i = 0; i < MAX_INDIRECT; i++) // 间接索引
        {
            size_curr = index_boundary.index_indir[i];
            if (offset < size_curr)
            {
                seek_cluster_offset = (size_t)(offset - size_prev);
                for (j = 0; j <= i; j++)
                {
                    seek_index[j + 1] = (size_t)(seek_cluster_offset / index_boundary.size_level[i - j]);
                    seek_cluster_offset %= index_boundary.size_level[i - j];
                }
                seek_index[0] = i;
                return i + 1;
            }
            size_prev = size_curr;
        }
    }
    else
    {
        size_prev = size_level_max;
    }

    return SEEK_FAIL;
}

bool Node::_LoadCluster(size_t offset)
{
    ClusterContainer* t_cluster = NULL;

    __int8 index_depth = ClusterSeek(offset);
    if (index_depth == SEEK_FAIL)
    {
        return false;
    }

    t_cluster = LoadClusterByIndex(index_depth, seek_index);

    if (t_cluster == NULL)
    {
        return false;
    }

    target_cluster = t_cluster;
    target_offset = offset;
    target_cluster_offset = seek_cluster_offset;
    return true;
}

ClusterContainer* Node::LoadClusterByIndex(__int8 depth, size_t* index)
{
    ClusterContainer* index_cluster = NULL;
    if (depth == -1)
    {
        ClusterContainer* next = node_mgr->cluster_service->Fetch(MC->GetCluster());
        ASSERT_NULL(next);

        return next;
    }
    if (depth == 0) // 直接索引
    {
        ClusterContainer* next = node_mgr->cluster_service->Fetch(inode.index_direct[index[0]]);
        ASSERT_NULL(next);

        return next;
    }

    // 间接索引
    cluster_t start_index;
    switch (depth)
    {
    case 1:
    case 2:
    case 3:
        start_index = inode.index_indir[depth - 1];
        break;
    default:
        goto faild;
    }

    index_cluster = node_mgr->cluster_service->Fetch(start_index);
    ASSERT_NULL(index_cluster);

    int i;
    cluster_t next_index;
    for (i = 1; i <= depth; i++)
    {
        ASSERT_SIZE(index_cluster->Read(0, index[i] * sizeof(cluster_t), sizeof(cluster_t), (unsigned __int8*)&next_index), sizeof(cluster_t)); // 从索引节点读出下一层的簇号
        node_mgr->cluster_service->Dispose(*index_cluster); //释放索引节点
        index_cluster = node_mgr->cluster_service->Fetch(next_index); // 获取下一层簇
        ASSERT_NULL(index_cluster);
    }

    return index_cluster;

faild:
    if (index_cluster != NULL)
    {
        node_mgr->cluster_service->Dispose(*index_cluster);
    }
    return NULL;
}

bool Node::LoadCluster(size_t offset)
{
    ASSERT_FALSE(_LoadCluster(offset)); // 加载目标簇

    // 释放当前簇，将读写指向新簇
    ClusterContainer* old = current;
    if (old != NULL) {
        old->Sync();
        node_mgr->cluster_service->Dispose(*old); //释放旧簇
    }
    current = target_cluster;
    cdo = target_offset;
    cco = target_cluster_offset;

    return true;
faild:
    return false;
}

#define DISPOSE_CLUSTER(_c) do{ClusterContainer* tmp_index = _c; _c = NULL;ASSERT_FALSE(node_mgr->cluster_service->Dispose(*tmp_index));}while(0)

#define SEEK_SIZE(s) \
do{                                                           \
    if ((seek_depth_##s = ClusterSeek(s - 1)) == SEEK_FAIL)       \
    {                                                         \
        goto faild;                                           \
    }                                                         \
    seek_cluster_offset_##s = seek_cluster_offset;            \
    memcpy(seek_index_##s, seek_index, sizeof(seek_index));   \
} while(0)

bool Node::Shrink(size_t curr, size_t target)
{
    size_t seek_index_curr[MAX_INDIRECT + 1];
    size_t seek_cluster_offset_curr; // 簇内偏移在收缩时用不到，在扩展时需要将簇上新扩展出的空间用0来填充
    __int8 seek_depth_curr;
    size_t seek_index_target[MAX_INDIRECT + 1];
    size_t seek_cluster_offset_target;
    __int8 seek_depth_target;

    SEEK_SIZE(curr);
    SEEK_SIZE(target);

    if (seek_depth_curr != seek_depth_target) // 处于不同层级，则先移除更高层级的全部结点
    {
        // 因为最低depth为-1，且curr和targer不相等，且此时是缩小，则seek_depth_curr必然>=0
        // 先处理间接索引
        if (seek_depth_curr > 0)
        {
            __int8 fin_depth;
            int i = 1;
            if (seek_depth_target > 0)
            {
                i = seek_depth_target + 1;
            }
            fin_depth = i - 1;
            for (; i < seek_depth_curr; i++)
            {
                ASSERT_FALSE(RemoveClusterRight(i, inode.index_indir[i - 1], NULL));
                inode.index_indir[i - 1] = EOC;
            }
            ASSERT_FALSE(RemoveClusterRight(i, inode.index_indir[i - 1], &seek_index_curr[1]));
            inode.index_indir[i - 1] = EOC;
            // 修正当前索引
            seek_depth_curr = fin_depth;
            //seek_cluster_offset_curr = index_boundary.size_level[0] - 1;
            if (seek_depth_curr == 0)
            {
                seek_index_curr[0] = MAX_DIRECT - 1;
            }
            else
            {
                seek_index_curr[0] = seek_depth_curr - 1;
                size_t s2 = index_boundary.index_per_cluster - 1;
                for (i = 1; i <= seek_depth_curr; i++)
                {
                    seek_index_curr[i] = s2;
                }
            }
        }
        // 处理直接索引
        if (seek_depth_target == -1) // 此时必有 seek_depth_curr == 0, seek_depth_target == -1
        {
            size_t i;
            for (i = 0; i <= seek_index_curr[0]; i++)
            {
                ASSERT_FALSE(RemoveClusterRight(0, inode.index_direct[i], NULL));
                inode.index_direct[i] = EOC;
            }
            // 修正当前索引
            seek_depth_curr = -1;
            //seek_cluster_offset_curr = index_boundary.size_level[0] - 1;
        }
    }

    // 处理处于同一层级的数据
    if (seek_depth_curr > 0)
    {// 间接索引
        // 判断是否在同一个簇中
        int i;
        bool same_cluster = true;
        for (i = 1; i <= seek_depth_curr; i++)
        {
            if (seek_index_curr[i] != seek_index_target[i])
            {
                same_cluster = false;
                break;
            }
        }
        if (!same_cluster) // 不在同一个簇中
        {
            // 删除多余簇
            ASSERT_FALSE(RemoveCluster(seek_depth_curr, inode.index_indir[seek_depth_curr - 1], &seek_index_target[1], &seek_index_curr[1]));
            //修正索引
            memcpy(seek_index_curr, seek_index_target, sizeof(seek_index_target));
            //seek_cluster_offset_curr = index_boundary.size_level[0] - 1;
        }
    }
    else if (seek_depth_curr == 0)
    { // 直接索引
        size_t i;
        for (i = seek_index_target[0] + 1; i <= seek_index_curr[0]; i++)
        {
            ASSERT_FALSE(RemoveClusterRight(0, inode.index_direct[i], NULL));
            inode.index_direct[i] = EOC;
        }
        // 修正当前索引
        //seek_index_target[0] = seek_index_curr[0];
        //seek_cluster_offset_curr = index_boundary.size_level[0] - 1;
    }

    // 最后处理在同一个簇中的情况
    // 额。。。貌似并不需要处理
    // 直接指针改一下就好了

    // 然后我们更新文件的描述
    inode.size = target;
    ASSERT_FALSE(Modify());

    return true;
faild:
    return false;
}

bool Node::Expand(size_t curr, size_t target)
{
    size_t seek_index_curr[MAX_INDIRECT + 1];
    size_t seek_cluster_offset_curr;
    __int8 seek_depth_curr;
    size_t seek_index_target[MAX_INDIRECT + 1];
    size_t seek_cluster_offset_target;
    __int8 seek_depth_target;

    cluster_t next_cluster;
    ClusterContainer* current_cluster = NULL;

    SEEK_SIZE(curr);
    SEEK_SIZE(target);

    if (seek_depth_curr != seek_depth_target)
    {
        // 先处理间接索引
        if (seek_depth_target > 0)
        {
            __int8 fin_depth;
            int i = 1;
            if (seek_depth_curr > 0)
            {
                i = seek_depth_curr + 1;
            }
            fin_depth = i - 1;
            for (; i < seek_depth_target; i++)
            {
                ASSERT_EOC(next_cluster = BuildClusterRight(i, NULL));
                inode.index_indir[i - 1] = next_cluster;
            }
            ASSERT_EOC(next_cluster = BuildClusterRight(i, &seek_index_target[1]));
            inode.index_indir[i - 1] = next_cluster;
            // 修正当前索引
            seek_depth_target = fin_depth;
            seek_cluster_offset_target = (cluster_size_t)index_boundary.size_level[0] - 1;
            if (seek_depth_target == 0)
            {
                seek_index_target[0] = MAX_DIRECT - 1;
            }
            else
            {
                seek_index_target[0] = seek_depth_target - 1;
                size_t s2 = index_boundary.index_per_cluster - 1;
                for (i = 1; i <= seek_depth_target; i++)
                {
                    seek_index_target[i] = s2;
                }
            }
        }
        // 处理直接索引
        if (seek_depth_curr == -1) // 此时必有 seek_depth_curr == -1, seek_depth_target == 0
        {
            size_t i;
            for (i = 0; i <= seek_index_target[0]; i++)
            {
                ASSERT_EOC(next_cluster = BuildClusterRight(0, NULL));
                inode.index_direct[i] = next_cluster;
            }
            // 修正当前索引
            seek_depth_target = -1;
            seek_cluster_offset_target = (cluster_size_t)index_boundary.size_level[0] - 1;
        }
    }

    // 处理处于同一层级的数据
    if (seek_depth_target > 0)
    {// 间接索引
        // 判断是否在同一个簇中
        int i;
        bool same_cluster = true;
        for (i = 1; i <= seek_depth_curr; i++)
        {
            if (seek_index_curr[i] != seek_index_target[i])
            {
                same_cluster = false;
                break;
            }
        }
        if (!same_cluster) // 不在同一个簇中
        {
            ASSERT_FALSE(BuildCluster(seek_depth_target, inode.index_indir[seek_depth_target - 1], &seek_index_curr[1], &seek_index_target[1]));
            //修正索引
            memcpy(seek_index_target, seek_index_curr, sizeof(seek_index_curr));
            seek_cluster_offset_target = (cluster_size_t)index_boundary.size_level[0] - 1;
        }
    }
    else if (seek_depth_target == 0)
    {
        size_t i;
        for (i = seek_index_curr[0] + 1; i <= seek_index_target[0]; i++)
        {
            ASSERT_EOC(next_cluster = BuildClusterRight(0, NULL));
            inode.index_direct[i] = next_cluster;
        }
        // 修正当前索引
        seek_index_target[0] = seek_index_curr[0];
        seek_cluster_offset_target = (cluster_size_t)index_boundary.size_level[0] - 1;
    }

    // 最后处理在同一个簇中的情况
    // 在这里需要将多出来的字节用0填充
    ASSERT_NULL(current_cluster = LoadClusterByIndex(seek_depth_target, seek_index_target));
    // 用0填充后续区域
    size_t new_size = seek_cluster_offset_target - seek_cluster_offset_curr;
    ASSERT_SIZE(current_cluster->Memset(seek_cluster_offset_curr + 1, new_size, 0), new_size);
    DISPOSE_CLUSTER(current_cluster);

    // 然后我们更新文件的描述
    inode.size = target;

    ASSERT_FALSE(Modify());

    return true;
faild:
    if (current_cluster != NULL)
    {
        node_mgr->cluster_service->Dispose(*current_cluster);
    }
    return false;
}

#undef SEEK_SIZE

#define READ_INDEX(_i) ASSERT_SIZE(current_index->Read(0, (_i) * sizeof(cluster_t), sizeof(cluster_t), (unsigned __int8*)&next_index), sizeof(cluster_t))
#define WRITE_INDEX(_i,_index) ASSERT_SIZE(current_index->Write((_i) * sizeof(cluster_t), 0, sizeof(cluster_t), (unsigned __int8*)&_index), sizeof(cluster_t))

bool Node::RemoveClusterRight(__int8 depth, cluster_t index, size_t* edge_right)
{
    ClusterContainer* current_index = NULL;
    cluster_t next_index;
    size_t i;
    if (depth > 0)
    {
        depth--;
        ASSERT_NULL(current_index = node_mgr->cluster_service->Fetch(index)); // 打开本层索引

        if (edge_right == NULL) // 不在边缘节点上
        {
            for (i = 0; i < index_boundary.index_per_cluster; i++)
            {
                READ_INDEX(i); // 读取第i个节点索引
                ASSERT_FALSE(RemoveClusterRight(depth, next_index, NULL));
            }
        }
        else
        {
            size_t edge_this = *edge_right;
            for (i = 0; i < edge_this; i++)
            {
                READ_INDEX(i); // 读取边缘之前的节点
                ASSERT_FALSE(RemoveClusterRight(depth, next_index, NULL));
            }
            READ_INDEX(edge_this); // 读取边缘节点索引
            ASSERT_FALSE(RemoveClusterRight(depth, next_index, &edge_right[1]));
        }

        DISPOSE_CLUSTER(current_index);// 关闭索引簇
    }

    ASSERT_FALSE(node_mgr->cluster_service->Free(index)); // 释放目标簇

    return true;
faild:
    if (current_index != NULL)
    {
        node_mgr->cluster_service->Dispose(*current_index);
    }
    return false;
}

bool Node::RemoveClusterLeft(__int8 depth, cluster_t index, size_t* edge_left)
{
    ClusterContainer* current_index = NULL;
    cluster_t next_index;
    size_t i;
    if (depth == 0)
    {
        return true;
    }
    depth--;

    ASSERT_NULL(current_index = node_mgr->cluster_service->Fetch(index)); // 打开本层索引

    for (i = *edge_left + 1; i < index_boundary.index_per_cluster; i++)
    {
        READ_INDEX(i); // 读取第i个节点索引
        ASSERT_FALSE(RemoveClusterRight(depth, next_index, NULL));
    }

    READ_INDEX(*edge_left); // 读取左边缘节点

    DISPOSE_CLUSTER(current_index); // 关闭索引簇

    ASSERT_FALSE(RemoveClusterLeft(depth, next_index, &edge_left[1])); // 左侧向下走

    return true;
faild:
    if (current_index != NULL)
    {
        node_mgr->cluster_service->Dispose(*current_index);
    }
    return false;
}

bool Node::RemoveCluster(__int8 depth, cluster_t index, size_t* edge_left, size_t* edge_right)
{
    ClusterContainer* current_index = NULL;
    cluster_t next_index;
    if (depth == 0)
    {
        goto succ;
    }
    depth--;

    ASSERT_NULL(current_index = node_mgr->cluster_service->Fetch(index)); // 打开本层索引

    if (*edge_left == *edge_right)
    {
        READ_INDEX(*edge_left);
        DISPOSE_CLUSTER(current_index); // 关闭索引簇

        ASSERT_FALSE(RemoveCluster(depth, next_index, &edge_left[1], &edge_right[1]));

        goto succ;
    }

    // 删除右侧独立分支
    size_t i;
    size_t edge_this = *edge_right;
    for (i = *edge_left + 1; i < edge_this; i++)
    {
        READ_INDEX(i); // 读取边缘之前的节点
        ASSERT_FALSE(RemoveClusterRight(depth, next_index, NULL));
    }
    READ_INDEX(edge_this); // 读取边缘节点索引
    ASSERT_FALSE(RemoveClusterRight(depth, next_index, &edge_right[1]));

    READ_INDEX(*edge_left); // 读取左边缘节点

    DISPOSE_CLUSTER(current_index); // 关闭索引簇

    ASSERT_FALSE(RemoveClusterLeft(depth, next_index, &edge_left[1])); // 左侧向下走

succ:
    return true;
faild:
    if (current_index != NULL)
    {
        node_mgr->cluster_service->Dispose(*current_index);
    }
    return false;
}

cluster_t Node::BuildClusterRight(__int8 depth, size_t* edge_right)
{
    ClusterContainer* current_index = NULL;
    cluster_t current_cluster = EOC;
    cluster_t next_index;
    size_t i;

    ASSERT_NULL(node_mgr->cluster_service->Allocate(1, &current_cluster));
    ASSERT_NULL(current_index = node_mgr->cluster_service->Fetch(current_cluster));

    if (depth > 0)
    {
        depth--;

        if (edge_right == NULL)
        {
            for (i = 0; i < index_boundary.index_per_cluster; i++)
            {
                ASSERT_EOC(next_index = BuildClusterRight(depth, NULL)); // 构造底层数据
                WRITE_INDEX(i, next_index);
            }
        }
        else
        {
            size_t edge_this = *edge_right;
            for (i = 0; i < edge_this; i++)
            {
                ASSERT_EOC(next_index = BuildClusterRight(depth, NULL)); // 构造底层数据
                WRITE_INDEX(i, next_index);
            }
            ASSERT_EOC(next_index = BuildClusterRight(depth, &edge_right[1])); // 构造底层数据
            WRITE_INDEX(edge_this, next_index);
        }
    }
    else
    {
        ASSERT_FALSE(current_index->Memset(0, (cluster_size_t)index_boundary.size_level[0], 0)); // 清空数据
    }

    DISPOSE_CLUSTER(current_index);

    return current_cluster;
faild:
    if (current_index != NULL)
    {
        node_mgr->cluster_service->Dispose(*current_index);
    }
    if (current_cluster != EOC)
    {
        node_mgr->cluster_service->Free(current_cluster);
    }
    return EOC;
}

bool Node::BuildClusterLeft(__int8 depth, cluster_t index, size_t* edge_left)
{
    ClusterContainer* current_index = NULL;
    cluster_t next_index;
    size_t i;
    if (depth == 0)
    {
        return true;
    }
    depth--;

    ASSERT_NULL(current_index = node_mgr->cluster_service->Fetch(index)); // 打开本层索引

    for (i = *edge_left + 1; i < index_boundary.index_per_cluster; i++)
    {
        ASSERT_EOC(next_index = BuildClusterRight(depth, NULL)); // 构造底层数据
        WRITE_INDEX(i, next_index);
    }

    READ_INDEX(*edge_left); // 读取左边缘节点

    DISPOSE_CLUSTER(current_index); // 关闭索引簇

    ASSERT_FALSE(BuildClusterLeft(depth, next_index, &edge_left[1])); // 左侧向下走

    return true;
faild:
    if (current_index != NULL)
    {
        node_mgr->cluster_service->Dispose(*current_index);
    }
    return false;
}

bool Node::BuildCluster(__int8 depth, cluster_t index, size_t* edge_left, size_t* edge_right)
{
    ClusterContainer* current_index = NULL;
    cluster_t next_index;
    if (depth == 0)
    {
        goto succ;
    }
    depth--;

    ASSERT_NULL(current_index = node_mgr->cluster_service->Fetch(index)); // 打开本层索引

    if (*edge_left == *edge_right)
    {
        READ_INDEX(*edge_left);
        DISPOSE_CLUSTER(current_index); // 关闭索引簇

        ASSERT_FALSE(BuildCluster(depth, next_index, &edge_left[1], &edge_right[1]));

        goto succ;
    }

    // 先创建右侧分支
    size_t i;
    size_t edge_this = *edge_right;
    for (i = *edge_left + 1; i < edge_this; i++)
    {
        ASSERT_EOC(next_index = BuildClusterRight(depth, NULL)); // 构造底层数据
        WRITE_INDEX(i, next_index);
    }
    ASSERT_EOC(next_index = BuildClusterRight(depth, &edge_right[1])); // 构造边缘节点
    WRITE_INDEX(i, next_index);

    READ_INDEX(*edge_left); // 读取左边缘节点

    DISPOSE_CLUSTER(current_index); // 关闭索引簇

    ASSERT_FALSE(BuildClusterLeft(depth, next_index, &edge_left[1])); // 左侧向下走
succ:
    return true;
faild:
    if (current_index != NULL)
    {
        node_mgr->cluster_service->Dispose(*current_index);
    }
    return false;
}

cluster_t Node::GetNodeId()
{
    return MC->GetCluster();
}

bool Node::Sync()
{
    if ((flag & FLAG_DIRTY) == 0)
    {
        return true;
    }

    inode.time_modify = time(NULL);
    // MC数据写入
    ASSERT_SIZE(MC->Write(0, 0, sizeof(INode), (unsigned __int8*)&inode), sizeof(INode));

    if (current != MC)
    {
        ASSERT_FALSE(current->Sync());
    }
    ASSERT_FALSE(MC->Sync());

    flag &= ~FLAG_DIRTY;
    flag &= ~MASK_DCOUNT;

    return true;
faild:
    return false;
}

unsigned __int16 Node::GetMode(unsigned __int16 mask)
{
    return inode.mode & mask;
}

bool Node::SetMode(unsigned __int16 mode, unsigned __int16 mask)
{
    mode &= mask;
    if (GetMode(mask) == mode)
    {
        return true;
    }

    // TODO: 增加权限和有效性验证

    inode.mode = inode.mode & ~mask | mode;

    ASSERT_FALSE(Modify());

    return true;
faild:
    return false;
}

size_t Node::GetSize()
{
    return inode.size;
}

bool Node::Truncate(size_t size)
{
    size_t s = GetSize();
    if (size == s)
    {
        return true;
    }

    return size > s ? Expand(s, size) : Shrink(s, size);
}

size_t Node::GetPointer()
{
    return cdo;
}

__int64 Node::Seek(size_t offset)
{
    if (offset == cdo)
    {
        return cdo;
    }
    if (offset < 0 || offset >= GetSize())
    {
        goto faild;
    }

    ASSERT_FALSE(LoadCluster((size_t)offset));

    return cdo;
faild:
    return EOF;
}

size_t Node::Write(void const* buffer, size_t size)
{
    // 先计算剩余空间够不够读取
    size_t max_size = GetSize() - cdo;
    if (max_size < size)
    {
        size = max_size;
    }

    if (size == 0)
    {
        return 0;
    }

    size_t offset = 0;
    size_t cluster_size = node_mgr->cluster_service->GetClusterSize();
    size_t available_in_cluster; // 当前簇可用数据

    while (size > 0)
    {
        available_in_cluster = cluster_size - cco;
        if (available_in_cluster > size)
        {
            available_in_cluster = size;
        }
        if (available_in_cluster == 0) // 当前簇没有空间可以写入
        {
            // 载入下一块
            ASSERT_FALSE(LoadCluster(cdo));
            continue;
        }
        ASSERT_SIZE(current->Write(cco, offset, available_in_cluster, (unsigned char*)buffer), available_in_cluster);

        cco += available_in_cluster;
        cdo += available_in_cluster;
        offset += available_in_cluster;
        size -= available_in_cluster;
    }
    Modify();

faild:
    return offset;
}

size_t Node::Read(void* buffer, size_t size)
{
    // 先计算剩余内容够不够读取
    size_t max_size = GetSize() - cdo;
    if (max_size < size)
    {
        size = max_size;
    }

    if (size == 0)
    {
        return 0;
    }

    size_t offset = 0;
    size_t cluster_size = node_mgr->cluster_service->GetClusterSize();
    size_t available_in_cluster; // 当前簇可用数据
    while (size > 0)
    {
        available_in_cluster = cluster_size - cco;
        if (available_in_cluster > size)
        {
            available_in_cluster = size;
        }
        if (available_in_cluster == 0) // 当前簇没有数据可以读取
        {
            // 载入下一块
            ASSERT_FALSE(LoadCluster(cdo));
            continue;
        }
        ASSERT_SIZE(current->Read(offset, cco, available_in_cluster, (unsigned char*)buffer), available_in_cluster);

        cco += available_in_cluster;
        cdo += available_in_cluster;
        offset += available_in_cluster;
        size -= available_in_cluster;
    }

faild:
    return offset;
}

NodeMgr::NodeMgr(ClusterMgr* cluster_mgr) :
    cluster_service(cluster_mgr)
{
    size_t cluster_size = cluster_mgr->GetClusterSize();
    size_t index_per_cluster = cluster_size / sizeof(cluster_t);

    index_boundary.index_per_cluster = index_per_cluster;
    index_boundary.size_level[0] = cluster_size;

    // 计算每一级索引支持的最大数据长度
    file_size_t s, s2;
    index_boundary.no_index = s = cluster_size - sizeof(INode);
    int i;
    for (i = 0; i < MAX_DIRECT; i++)
    {
        s += cluster_size;
        index_boundary.index_direct[i] = s;
    }

    s2 = cluster_size * index_per_cluster;
    s += s2;
    for (i = 0; i < MAX_INDIRECT; i++)
    {
        index_boundary.index_indir[i] = s;
        index_boundary.size_level[i + 1] = s2;
        s2 *= index_per_cluster;
        s += s2;
    }
}

Node* NodeMgr::CreateRootNode()
{
    ClusterContainer* cluster = cluster_service->Fetch(CLUSTER_REV_SECONDARY);
    ASSERT_NULL(cluster);

    Node* node = CreateNode(cluster);
    ASSERT_NULL(node);

    return node;
faild:
    if (cluster != NULL)
    {
        cluster_service->Dispose(*cluster);
    }
    return NULL;
}

Node* NodeMgr::OpenRootNode()
{
    return OpenNode(CLUSTER_REV_SECONDARY);
}

Node* NodeMgr::CreateNode(ClusterContainer* mc)
{
    Node* node = new Node(this, mc);
    ASSERT_NULL(node);

    ASSERT_FALSE(node->Init());

    return node;
faild:
    DELETE(node);
    return NULL;
}

Node* NodeMgr::CreateNode()
{
    cluster_t node_cluster[1];
    node_cluster[0] = EOC;

    ASSERT_NULL(cluster_service->Allocate(1, node_cluster));

    ClusterContainer* cluster = cluster_service->Fetch(node_cluster[0]);
    ASSERT_NULL(cluster);

    Node* node = CreateNode(cluster);
    ASSERT_NULL(node);

    return node;
faild:
    if (cluster != NULL)
    {
        cluster_service->Dispose(*cluster);
    }
    cluster_service->Free(node_cluster[0]);
    return NULL;
}

Node* NodeMgr::OpenNode(cluster_t n)
{
    Node* node = NULL;
    ClusterContainer* cluster = cluster_service->Fetch(n);
    ASSERT_NULL(cluster);

    node = new Node(this, cluster);
    ASSERT_NULL(node);

    ASSERT_FALSE(node->Load());

    return node;
faild:
    DELETE(node);
    if (cluster != NULL)
    {
        cluster_service->Dispose(*cluster);
    }
    return NULL;
}

bool NodeMgr::Close(Node* node)
{
    bool ret = true;

    ret = node->Sync() && ret;
    ret = cluster_service->Dispose(*node->current) && ret;
    ret = cluster_service->Dispose(*node->MC) && ret;

    delete node;

    return ret;
}

bool NodeMgr::Delete(Node* node)
{
    cluster_t mc = node->MC->GetCluster();

    ASSERT_FALSE(node->Truncate(0)); // 先将文件尺寸缩小为0，以释放引用的数据簇
    ASSERT_FALSE(Close(node)); // 再关闭 node
    ASSERT_FALSE(cluster_service->Free(mc));// 最后释放掉 MC 占用的簇

    return true;
faild:
    return false;
}

bool NodeMgr::Sync()
{
    return false;
}
