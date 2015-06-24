#ifndef __VF_H_FFS
#define __VF_H_FFS

#include "cluster.h"
#include "node.h"

typedef struct
{
    __int8 vflag;
    size_t cfo; // current file offset£¬ÎÄ¼þÖ¸Õë
    Node* node;
} vfile_t;

class VFile
{
private:
    NodeMgr* node_service;

    vfile_t* Open(Node* node);
public:
    explicit VFile(NodeMgr* node_mgr);

    vfile_t* Open(cluster_t node);
    vfile_t* Create();
    bool Close(vfile_t* vf);

    __int64 Truncate(vfile_t* vf, size_t size);
    __int64 Seek(vfile_t* vf, __int64 offset, __int32 origin);
    size_t Tell(vfile_t* vf);
    size_t Write(vfile_t* vf, void const* buffer, size_t size);
    size_t Read(vfile_t* vf, void* buffer, size_t size);
};

#endif
