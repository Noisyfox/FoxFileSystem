
template<typename K, typename V>
class LruCache
{
private:
    struct Node
    {
        K key;
        V value;
        Node* prev;
        Node* next;
    };

    size_t size;
    size_t count;

    Node cache;
    Node* it;

    Node* free_node;

    void Recycle(Node* node)
    {
        node->next = free_node;
        free_node = node;
    }

    Node* Obtain()
    {
        if (free_node == NULL)
        {
            return new Node();
        }
        else
        {
            Node* n = free_node;
            free_node = n->next;
            return n;
        }
    }

    void Cut(Node* node)
    {
        Node* prev = node->prev;
        Node* next = node->next;
        prev->next = next;
        next->prev = prev;
    }

    void Link(Node* prev, Node* node)
    {
        Node* next = prev->next;
        node->prev = prev;
        node->next = next;
        prev->next = node;
        next->prev = node;
    }

public:
    LruCache(size_t size = 512) :size(size), count(0), free_node(NULL)
    {
        memset(&cache, 0, sizeof(Node));
        cache.prev = cache.next = &cache;
    }

    ~LruCache()
    {
        while (Pop() != NULL);

        Node* n = free_node;
        while (n != NULL)
        {
            Node* nn = n->next;
            delete n;
            n = nn;
        }
    }

    V Push(K key, V value)
    {
        if (value == NULL)
        {
            return NULL;
        }

        V freed_value = NULL;
        Node* new_node;

        if (size == count)
        {
            new_node = cache.prev;
            Cut(new_node);
            freed_value = new_node->value;
        }
        else
        {
            new_node = Obtain();
            count++;
        }

        new_node->key = key;
        new_node->value = value;

        Link(&cache, new_node);

        return freed_value;
    }

    V Pop()
    {
        if (count == 0)
        {
            return NULL;
        }

        Node* least = cache.prev;
        Cut(least);
        count--;
        V value = least->value;
        Recycle(least);

        return value;
    }

    V Hit(K key)
    {
        if (count == 0)
        {
            return NULL;
        }

        Node* sentinel = &cache;
        Node* node = cache.next;
        do
        {
            if (node->key == key)
            {
                Cut(node);
                count--;
                V value = node->value;
                Recycle(node);
                return value;
            }
            node = node->next;
        } while (node != sentinel);

        return NULL;
    }

    // ¼òÒ×µü´úÆ÷
    V Next(bool first)
    {
        if(first)
        {
            it = cache.next;
        }

        Node* sentinel = &cache;

        if(it != sentinel)
        {
            V value = it->value;
            it = it->next;

            return value;
        }

        return NULL;
    }
};
