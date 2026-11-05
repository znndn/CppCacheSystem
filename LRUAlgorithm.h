
#pragma once

#include <memory>
#include <unordered_map>
#include <mutex>
#include "AlgorithmStandard.h"

namespace LRU
{
    template<typename Key,typename Value>
    class LRUAlgorithm;

    template<typename Key,typename Value>
    class LRUNode
    {
        friend class LRUAlgorithm<Key,Value>;

        private:

        Key key;
        Value value;

        std::shared_ptr<LRUNode> next;
        std::weak_ptr<LRUNode> prev;
        // 反向链接使用弱引用，避免相邻节点形成所有权环。

        public:

        LRUNode(Key key,Value val):key(key),value(val)
        {
            next=nullptr;
            prev.reset();
        }

        Key getKey() const
        {
            return key;
        }
        Value getValue() const
        {
            return value;
        }
        void setValue(Value val)
        {
            value=val;
        }
        void setKey(Key key)
        {
            this->key=key;
        }
    };

    template<typename Key,typename Value>
    class LRUAlgorithm final : public AlgorithmStandard::Algorithmstandard<Key,Value>
    {
        private:

        std::unordered_map<Key,std::shared_ptr<LRUNode<Key,Value> > > cache;
        // 哈希表定位节点，链表按访问时间维护淘汰顺序。

        // 头哨兵之后的节点最久未访问，尾哨兵之前的节点最近访问。
        std::shared_ptr<LRUNode<Key,Value>> dummyhead;
        std::shared_ptr<LRUNode<Key,Value>> dummytail;

        std::mutex mutex;

        void addNodeToLast(std::shared_ptr<LRUNode<Key,Value> > node)
        {
            if (auto dummytail_prev_shareptr=dummytail->prev.lock())
            {
                dummytail_prev_shareptr->next=node;
            }
            node->prev=dummytail->prev;
            node->next=dummytail;
            dummytail->prev=node;
        }

        void removeNode(std::shared_ptr<LRUNode<Key,Value> > node)
        {
            node->next->prev=node->prev;
            if (auto node_prev_sharedptr=node->prev.lock())
                node_prev_sharedptr->next=node->next;
            node->next=nullptr;
            node->prev.reset();
        }

        public:

        explicit LRUAlgorithm()
        {
            dummyhead=std::make_shared<LRUNode<Key,Value> >(Key{}, Value{});
            dummytail=std::make_shared<LRUNode<Key,Value> >(Key{}, Value{});

            dummyhead->next=dummytail;
            dummytail->prev=dummyhead;
        }

        ~LRUAlgorithm() override=default;

        bool get(const Key& key, Value& value) override
        {
            std::lock_guard lock(mutex);
            auto iter=cache.find(key);
            if (iter!=cache.end())
            {
                auto node_ptr=cache[key];
                value=node_ptr->getValue();
                removeNode(node_ptr);
                addNodeToLast(node_ptr);
                return true;
            }
            return false;
        }

        void put(const Value& val,const Key& key) override
        {
            std::lock_guard lock(mutex);
            auto iter=cache.find(key);
            if (iter!=cache.end())
            {
                // 更新已有键也会刷新访问顺序。
                auto node=cache[key];
                node->setValue(val);
                removeNode(cache[key]);
                addNodeToLast(cache[key]);
            }
            else
            {
                if (DEFAULT_CACHE_CAPACITY<=cache.size())
                {
                    auto NodeToDelete=dummyhead->next;
                    cache.erase(NodeToDelete->getKey());
                    removeNode(NodeToDelete);

                    auto newNode=std::make_shared<LRUNode<Key,Value> >(key,val);
                    cache[key]=newNode;
                    addNodeToLast(newNode);
                }
                else
                {
                    auto NewNode=std::make_shared<LRUNode<Key,Value> >(key,val);
                    cache[key]=NewNode;
                    addNodeToLast(NewNode);
                }
            }
        }
    };
};