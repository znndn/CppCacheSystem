
#pragma once

#include <climits>
#include <memory>
#include <unordered_map>
#include <mutex>
#include "AlgorithmStandard.h"

namespace LFU
{
    template<typename Key,typename Value>
    class LFUAlgorithm;

    template <typename Key,typename Value>
    class Node
    {
        public:

        Key key;
        Value value;
        int NodeFrequency;

        std::shared_ptr<Node> next;
        std::weak_ptr<Node> prev;

        Node():NodeFrequency(1),next(nullptr){}
        Node(Key key,Value value):key(key),value(value),NodeFrequency(1),next(nullptr){}
    };

    // 新插入或迁入的节点放在频率桶尾部。
    template <typename Key,typename Value>
    class FreqList
    {
        friend class LFUAlgorithm<Key,Value>;

        private:

        int ListFrequency;
        std::shared_ptr<Node<Key,Value>> head;
        std::shared_ptr<Node<Key,Value>> tail;
        
        public:
        explicit FreqList(const int freq)
        {
            ListFrequency=freq;
            head=std::make_shared<Node<Key,Value>>();
            tail=std::make_shared<Node<Key,Value>>();
            head->next=tail;
            tail->prev=std::weak_ptr<Node<Key,Value>>(head);
        }

        bool isEmpty()
        {
            if (head==nullptr||head->next==tail)
            {
                return true;
            }
            return false;
        }
        void addNodeToCurrTail(std::shared_ptr<Node<Key,Value>> node)
        {
            node->next=tail;
            node->prev=tail->prev;
            if (auto tailPrevPtr=tail->prev.lock())
            {
                tailPrevPtr->next=node;
            }
            tail->prev=node;
        }
        void removeNodeFromCurrList(std::shared_ptr<Node<Key,Value> > node)
        {
            if (node->next == nullptr)
            {
                return;
            }
            node->next->prev=node->prev;
            if (auto node_prev_sharedptr=node->prev.lock())
                node_prev_sharedptr->next=node->next;

            node->next=nullptr;
            node->prev.reset();
        }
        std::shared_ptr<Node<Key,Value>> getCurrFirstNode()
        {
            if (isEmpty()==false)
            {
                return head->next;
            }
            return nullptr;
        }
    };

    template <typename Key,typename Value>
    class LFUAlgorithm final :public AlgorithmStandard::Algorithmstandard<Key,Value>
    {
        // 频率索引用于定位淘汰桶，键索引用于定位缓存节点。
        std::unordered_map<int,std::unique_ptr<FreqList<Key,Value>>> FrequencyList;
        std::unordered_map<Key,std::shared_ptr<Node<Key,Value>>> cache;
        int minFrequency;
        // get 和 put 在同一把锁内更新索引、链表和频率计数。
        std::mutex mutex;

        int threshold;
        int currentAverageNumber;
        int currentTotalNumber;

        private:

        int LfuAgingCount=0;

        void AddNodeToNewFrequencyList(std::shared_ptr<Node<Key,Value>> node)
        {
            int freq = node->NodeFrequency;
            if (FrequencyList.contains(freq) == false || FrequencyList[freq] == nullptr)
            {
                FrequencyList[freq] = std::make_unique<FreqList<Key,Value>>(freq);
            }
            FrequencyList[freq]->addNodeToCurrTail(node);
        }
        // 淘汰最低频率桶的首节点。
        void DeleteOldNode()
        {
            if (FrequencyList.contains(minFrequency)==false)
            {
                UpdateMinfrequency();
                if (FrequencyList.contains(minFrequency)==false) return;
            }
            auto list = FrequencyList[minFrequency].get();
            if (list==nullptr || list->isEmpty()) return;
            auto NodeToDelete = list->getCurrFirstNode();
            if (NodeToDelete==nullptr)
            {
                FrequencyList.erase(minFrequency);
                UpdateMinfrequency();
                return;
            }
            list->removeNodeFromCurrList(NodeToDelete);
            cache.erase(NodeToDelete->key);
            currentTotalNumber -= NodeToDelete->NodeFrequency;
            if (cache.size() == 0)
                currentAverageNumber = 0;
            else
                currentAverageNumber = currentTotalNumber / cache.size();
        }
        void NodeFreqUpgrade(std::shared_ptr<Node<Key,Value>> node)
        {
            int OldFrequency = node->NodeFrequency;
            if (FrequencyList.contains(OldFrequency))
            {
                auto oldList = FrequencyList[OldFrequency].get();
                if (oldList!=nullptr)
                {
                   oldList->removeNodeFromCurrList(node);
                }
            }
            node->NodeFrequency+=1;
            AddNodeToNewFrequencyList(node);
            addFrequencyCount();
        }
        void NewNodeInsert(Key key,Value value)
        {
            if (cache.size() >= DEFAULT_CACHE_CAPACITY)
            {
                DeleteOldNode();
            }
            auto NewNode=std::make_shared<Node<Key,Value>>(key,value);
            minFrequency = 1;
            AddNodeToNewFrequencyList(NewNode);
            cache[key]=NewNode;
            addFrequencyCount();
        }

        public:
        explicit LFUAlgorithm(const int threshold):
            minFrequency(INT_MAX),
            threshold(threshold),currentAverageNumber(0),currentTotalNumber(0)
        {
        }
        ~LFUAlgorithm() override = default;

        int GetLfuAgingCount()
        {
            return LfuAgingCount;
        }

        bool get(const Key& key, Value& value) override
        {
            std::lock_guard lock(mutex);
            if (cache.contains(key))
            {
                auto Nodeptr=cache.find(key)->second;
                // 记住迁移前的频率，便于清理迁移后变空的桶。
                int OldFrequency=Nodeptr->NodeFrequency;
                NodeFreqUpgrade(Nodeptr);
                if (FrequencyList.contains(OldFrequency))
                {
                    auto oldList=FrequencyList[OldFrequency].get(); 
                    if (oldList!=nullptr && oldList->isEmpty())
                    {
                        FrequencyList.erase(OldFrequency);
                        if (OldFrequency == minFrequency)
                        {
                            UpdateMinfrequency();
                        }
                    }
                }
                value=Nodeptr->value;
                return true;
            }
            return false;
        }
        void put(const Value& val,const Key& key) override
        {
            std::lock_guard lock(mutex);
            auto CacheIter=cache.find(key);
            if (CacheIter!=cache.end())
            {
                CacheIter->second->value=val;
                int OldFrequency=CacheIter->second->NodeFrequency;
                NodeFreqUpgrade(CacheIter->second);
                if (FrequencyList.contains(OldFrequency))
                {
                    auto oldList=FrequencyList[OldFrequency].get();
                    if (oldList!=nullptr && oldList->isEmpty())
                    {
                        FrequencyList.erase(OldFrequency);

                        if (OldFrequency == minFrequency)
                        {
                            UpdateMinfrequency();
                        }
                    }
                }
            }
            else
            {
                NewNodeInsert(key,val);
            }
        }
        void UpdateMinfrequency()
        {
            minFrequency=INT_MAX;
            for (const auto& map_pair : FrequencyList)
            {
                if (map_pair.second != nullptr && map_pair.second->isEmpty()==false && map_pair.first < minFrequency)
                {
                    minFrequency=map_pair.first;
                }
            }

            if (minFrequency == INT_MAX)
            {
                minFrequency = 1;
            }
        }
        void addFrequencyCount()
        {
            currentTotalNumber++;
            if (cache.size()==0) currentAverageNumber=1;
            else currentAverageNumber=currentTotalNumber/cache.size();
            // 平均频率超过阈值时统一降频，减弱历史热点的长期优势。
            if (currentAverageNumber>threshold) ReduceAllNodeFrequency();
        }
        // 降频步长为平均频率的一半且至少为 1，频率下限为 1。
        // 同桶的迁移节点按哈希表的遍历顺序追加。
        void ReduceAllNodeFrequency()
        {
            int ValueToReduce = currentAverageNumber / 2;
            int ReducedFrequency = 0;
            LfuAgingCount++;

            if (ValueToReduce == 0 && currentAverageNumber > 0)
            {
                ValueToReduce = 1;
            }
            else if (ValueToReduce == 0)
            {
                return;
            }

            for (const auto& map_pair : cache)
            {
                auto node = map_pair.second;
                int NodeOldFrequency = node->NodeFrequency;

                if (NodeOldFrequency == 1) continue;

                if (FrequencyList.contains(NodeOldFrequency))
                {
                    auto oldList = FrequencyList[NodeOldFrequency].get();
                    if (oldList != nullptr)
                    {
                        oldList->removeNodeFromCurrList(node);
                    }
                }

                if (NodeOldFrequency - ValueToReduce < 1)
                {
                    ReducedFrequency +=  NodeOldFrequency - 1;
                    node->NodeFrequency = 1;
                }
                else
                {
                    ReducedFrequency += ValueToReduce;
                    node->NodeFrequency -= ValueToReduce;
                }

                AddNodeToNewFrequencyList(node);
            }

            for (auto it = FrequencyList.begin(); it != FrequencyList.end();)
            {
                FreqList<Key, Value>* list = it->second.get();
                if (list == nullptr || list->isEmpty())
                {
                    it = FrequencyList.erase(it);
                }
                else
                {
                    ++it;
                }
            }
            UpdateMinfrequency();
            currentTotalNumber -= ReducedFrequency;

            if (cache.size() == 0) currentAverageNumber = 0;
            else currentAverageNumber = currentTotalNumber / cache.size();
        }
    };
}