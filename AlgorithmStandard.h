
#pragma once

inline constexpr int DEFAULT_CACHE_CAPACITY=20;
inline constexpr int OPERATIONS=500000;
inline constexpr int HOTKEY=20;
inline constexpr int COLDKEYS=5000;

namespace AlgorithmStandard
{

    // LRU 与 LFU 共用的缓存策略接口。
    template<typename Key,typename Value>

    class Algorithmstandard
    {
    public:
        Algorithmstandard()=default;
        virtual ~Algorithmstandard()=default;
    
        // 命中时写入 value 并返回 true。
        virtual bool get (const Key& key, Value& value) = 0;

        virtual void put (const Value& val,const Key& key) = 0;
    };
}
