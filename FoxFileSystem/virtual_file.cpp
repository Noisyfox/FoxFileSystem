#include "virtual_file.h"
#include "util.h"

#define VFLAG_OVERSIZE 0x1

vfile_t* VFile::Open(Node* node)
{
    vfile_t* vf = NULL;
    ASSERT_NULL(vf = new vfile_t);

    vf->vflag = 0;
    vf->cfo = 0;
    vf->node = node;

    return vf;
faild:
    DELETE(vf);
    return NULL;
}

VFile::VFile(NodeMgr* node_mgr) :
    node_service(node_mgr)
{
}

vfile_t* VFile::Open(cluster_t node)
{
    Node* fileNode = NULL;
    vfile_t* vf = NULL;
    ASSERT_NULL(fileNode = node_service->OpenNode(node));

    ASSERT_NULL(vf = Open(fileNode));

    return vf;
faild:
    if (fileNode != NULL)
    {
        node_service->Close(fileNode);
    }
    return NULL;
}

vfile_t* VFile::Create()
{
    Node* fileNode = NULL;
    vfile_t* vf = NULL;
    ASSERT_NULL(fileNode = node_service->CreateNode());

    ASSERT_NULL(vf = Open(fileNode));

    return vf;
faild:
    if (fileNode != NULL)
    {
        node_service->Delete(fileNode);
    }
    return NULL;
}

bool VFile::Close(vfile_t* vf)
{
    if (vf == NULL)
    {
        return false;
    }

    bool ret = node_service->Close(vf->node);

    delete vf;

    return ret;
}

bool VFile::Delete(vfile_t* vf)
{
    Node* node = vf->node;
    Close(vf);
    return node_service->Delete(node);
}

__int64 VFile::Truncate(vfile_t* vf, size_t size)
{
    if (size < 0)
    {
        return EOF;
    }

    size_t node_pointer = vf->node->GetPointer();
    if (node_pointer >= size)
    {
        if (size == 0)
        {
            ASSERT_SIZE(vf->node->Seek(0), 0);
        }
        else
        {
            ASSERT_SIZE(vf->node->Seek(size - 1), size - 1);
        }
    }

    if (vf->cfo >= size)
    {
        vf->vflag |= VFLAG_OVERSIZE;
    }
    else
    {
        vf->vflag &= ~VFLAG_OVERSIZE;
    }

    return vf->node->Truncate(size);
faild:
    return EOF;
}

size_t VFile::Tell(vfile_t* vf)
{
    return vf->cfo;
}

__int64 VFile::Seek(vfile_t* vf, __int64 offset, __int32 origin)
{
    // 计算目标偏移
    __int64 target_offset;
    size_t node_size = vf->node->GetSize();

    switch (origin)
    {
    case SEEK_SET:
        target_offset = offset;
        break;
    case SEEK_CUR:
        target_offset = vf->cfo + offset;
        break;
    case SEEK_END:
        target_offset = node_size + offset;
        break;
    default:
        goto faild;
    }

    if (target_offset < 0)
    {
        goto faild;
    }

    if (target_offset >= node_size)
    {
        // over size，文件目标大小大于实际大小，暂时存下，等到下一次write的时候再实际改变文件大小
        vf->vflag |= VFLAG_OVERSIZE;
    }
    else
    {
        ASSERT_EOF(vf->node->Seek((size_t)target_offset));
        vf->vflag &= ~VFLAG_OVERSIZE;
    }

    vf->cfo = (size_t)target_offset;

    return (size_t)target_offset;
faild:
    return EOF;
}

size_t VFile::Write(vfile_t* vf, void const* buffer, size_t size)
{
    if (size <= 0)
    {
        return 0;
    }

    // 写入后大小
    size_t final_size = vf->cfo + size;
    size_t node_size = vf->node->GetSize();
    if (final_size > node_size)
    { // 尺寸扩展
        ASSERT_FALSE(vf->node->Truncate(final_size));
    }

    ASSERT_EOF(vf->node->Seek(vf->cfo));

    size_t write_size = vf->node->Write(buffer, size);
    vf->cfo += write_size;
    vf->vflag &= ~VFLAG_OVERSIZE;

    return write_size;
faild:
    return 0;
}

size_t VFile::Read(vfile_t* vf, void* buffer, size_t size)
{
    if (size <= 0 || (vf->vflag & VFLAG_OVERSIZE) != 0)
    {
        return 0;
    }

    size_t read_size = vf->node->Read(buffer, size);
    vf->cfo += read_size;

    return read_size;
}
