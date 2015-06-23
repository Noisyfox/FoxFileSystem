#ifndef __SPARSEARRAY_H_FFS
#define __SPARSEARRAY_H_FFS

#include <cstdlib>
#include <cstring>

template<typename K, typename V>
class SparseArray
{
private:
    struct Node
    {
        unsigned __int8 flag;
        K key;
        V value;
    };

    Node* data;
    Node* it;
    size_t size;
    size_t count;

    struct SearchResult
    {
        Node* target;
        Node* first_free;
        size_t empty_count;
    };

    void Trim()
    {
        Node* end = data + size;
        Node* n = data;
        Node* pr = data;

        while (n != end)
        {
            if (n->flag != 0)
            {
                *pr = *n;
                n->flag = 0;
                ++pr;
            }
            ++n;
        }
        while (pr != end)
        {
            pr->flag = 0;
            ++pr;
        }

        size_t new_size = count * 2 + 1;
        if (new_size < 128) new_size = 128;
        if(new_size < size)
        {
            size = new_size;
            data = (Node*)realloc((void*)data, size * sizeof(Node));
        }
    }

    void Search(K key, SearchResult* result)
    {
        Node* end = data + size;
        Node* n = data;
        Node* f = NULL;
        Node* target = NULL;

        size_t set_count = 0;
        size_t empty_count = 0;

        while (n != end)
        {
            if (n->flag != 0) // 当前项已占用
            {
                if (n->key == key) // 如果当前项的 key 就是要查找的 key
                {
                    target = n;
                    break;
                }
                set_count++;
            }
            else // 当前项是空的
            {
                if (f == NULL)
                {
                    f = n; // 找到的第一个空位
                }

                if (set_count >= count) // 如果已经找到了 count 个已占用位，则说明遍历完成，可以忽略之后的内容
                {
                    break;
                }

                empty_count++;
            }
            ++n;
        }

        result->target = target;
        result->first_free = f;
        result->empty_count = empty_count;
    }
public:
    SparseArray(size_t initial = 128) :
        size(initial),
        count(0)
    {
        size_t s = initial * sizeof(Node);
        data = (Node*)malloc(s);
        memset((void*)data, 0, s);
    }

    ~SparseArray()
    {
        free((void*)data);
    }

    void Set(K key, V value)
    {
        SearchResult result;

        Search(key, &result);

        if (result.target != NULL)
        {
            result.target->value = value;
        }
        else
        {
            if (result.first_free != NULL)
            {
                result.first_free->flag = 1;
                result.first_free->key = key;
                result.first_free->value = value;
                count++;
            }
            else
            {// 没有找到，则一定是满的，扩充
                size_t new_size = count * 2 + 1;
                data = (Node*)realloc((void*)data, new_size * sizeof(Node));
                Node* f = data + size;
                memset(&f, 0, count * sizeof(Node));

                size = new_size;

                f->flag = 1;
                f->key = key;
                f->value = value;
                count++;
            }
        }
        // 如果碎片过多，则需要重新压缩
        if (result.empty_count > count / 2)
        {
            Trim();
        }
    }

    V Get(K key)
    {
        SearchResult result;
        V value = NULL;

        Search(key, &result);
        if(result.target != NULL)
        {
            value = result.target->value;
        }

        // 如果碎片过多，则需要重新压缩
        if (result.empty_count > count / 2)
        {
            Trim();
        }

        return value;
    }

    void Remove(K key)
    {
        SearchResult result;

        Search(key, &result);
        if (result.target != NULL)
        {
            result.target->flag = 0;
            count--;
        }

        // 如果碎片过多，则需要重新压缩
        if (result.empty_count > count / 2)
        {
            Trim();
        }
    }

    // 简易迭代器
    V Next(bool first)
    {
        if(first)
        {
            it = data;
        }
        Node* end = data + size;

        while(it != end)
        {
            if(it->flag != 0)
            {
                V value = it->value;
                ++it;
                return value;
            }
            ++it;
        }
        return NULL;
    }
};

#endif
