#include <iostream>
#include <chrono>
#include <iomanip>
#include <climits>
#include <random>
#include "LRUAlgorithm.h"
#include "AlgorithmStandard.h"
#include "LFUAlgorithm.h"

namespace TEST
{
    void printResult(int operations,int hits, const std::string& description);

    class Timer
    {
        private:
        
        std::chrono::high_resolution_clock::time_point start;
        std::chrono::high_resolution_clock::time_point end;
        
        public:

        void TimerStart()
        {
            start = std::chrono::high_resolution_clock ::now();
        }
        std::chrono::milliseconds TimerEnd()
        {
            end = std::chrono::high_resolution_clock ::now();
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
            return duration;
        }
    };

    void TestAlgorithm()
    {
        // 三种策略处理同一操作序列，两个 LFU 实例对照老化效果。
        LRU::LRUAlgorithm<int,std::string> lru;
        LFU::LFUAlgorithm<int,std::string> lfuNoReduction(INT_MAX);
        LFU::LFUAlgorithm<int,std::string> lfuWithReduction(100); 

        int hits_LRU=0;
        int hits_LFU=0;
        int hits_LFU_AGING=0;
        int operations=0;

        std::random_device seed;
        std::mt19937 rng(seed());

        // 30% 写入、70% 读取，每次访问有 70% 的概率选中热点键。
        std::uniform_int_distribution GetOrPut(1,10);
        std::uniform_int_distribution HotOrCold(1,10);
        std::uniform_int_distribution HotKeyGen(0,HOTKEY-1);
        std::uniform_int_distribution ColdKeyGen(HOTKEY, HOTKEY + COLDKEYS - 1);

        // 用同一批热点键为三个缓存预热。
        for (int i=0;i<HOTKEY;i++)
        {
            std::string ToBeValue = "value" + std::to_string(i);
            lru.put(ToBeValue,i);
            lfuNoReduction.put(ToBeValue,i);
            lfuWithReduction.put(ToBeValue,i);
        }


        Timer t;
        t.TimerStart();
        for (int i=0;i<OPERATIONS;i++)
        {
            if (GetOrPut(rng)<=3)
            {
                if (HotOrCold(rng)<=3)
                {
                    int CurrentKey = ColdKeyGen(rng);

                    lru.put("value"+std::to_string(CurrentKey),CurrentKey);
                    lfuNoReduction.put("value"+std::to_string(CurrentKey),CurrentKey);
                    lfuWithReduction.put("value"+std::to_string(CurrentKey),CurrentKey);
                }
                else
                {
                    int CurrentKey = HotKeyGen(rng);

                    lru.put("value"+std::to_string(CurrentKey),CurrentKey);
                    lfuNoReduction.put("value"+std::to_string(CurrentKey),CurrentKey);
                    lfuWithReduction.put("value"+std::to_string(CurrentKey),CurrentKey);
                }
            }
            else // 命中率的分母只计入 get 请求。
            {
                operations++;
                if (HotOrCold(rng)<=3)
                {
                    int CurrentKey = ColdKeyGen(rng);
                    std::string retrived_value;
                    if (lru.get(CurrentKey,retrived_value)==true)
                    {
                        hits_LRU++;
                        if (retrived_value!="value"+std::to_string(CurrentKey))
                        {
                            std::cout<<"LRU 算法 get 热数据时,读取错误在"<<retrived_value<<std::endl;
                        }
                    }
                    if (lfuNoReduction.get(CurrentKey,retrived_value)==true)
                    {
                        hits_LFU++;
                        if (retrived_value!="value"+std::to_string(CurrentKey))
                        {
                            std::cout<<"LFU 无衰减 get 热数据时,读取错误在"<<retrived_value<<std::endl;
                        }
                    }
                    if (lfuWithReduction.get(CurrentKey,retrived_value)==true)
                    {
                        hits_LFU_AGING++;
                        if (retrived_value!="value"+std::to_string(CurrentKey))
                        {
                            std::cout<<"LFU 有衰减 get 热数据时,读取错误在"<<retrived_value<<std::endl;
                        }
                    }
                }
                else
                {
                    int CurrentKey = HotKeyGen(rng);
                    std::string retrived_value;
                    if (lru.get(CurrentKey,retrived_value)==true)
                    {
                        hits_LRU++;
                        if (retrived_value!="value"+std::to_string(CurrentKey))
                        {
                            std::cout<<"LRU get 读取冷数据时,读取错误在"<<retrived_value<<std::endl;
                        }
                    }
                    if (lfuNoReduction.get(CurrentKey,retrived_value)==true)
                    {
                        hits_LFU++;
                        if (retrived_value!="value"+std::to_string(CurrentKey))
                        {
                            std::cout<<"LFU 无衰减 get 冷数据时,读取错误在"<<retrived_value<<std::endl;
                        }
                    }
                    if (lfuWithReduction.get(CurrentKey,retrived_value)==true)
                    {
                        hits_LFU_AGING++;
                        if (retrived_value!="value"+std::to_string(CurrentKey))
                        {
                            std::cout<<"LFU 有衰减 get 冷数据时,读取错误在"<<retrived_value<<std::endl;
                        }
                    }
                }
            }
        }
        printResult(operations,hits_LRU,"LRU");
        printResult(operations,hits_LFU,"LFU无衰减");
        printResult(operations,hits_LFU_AGING,"LFU有衰减");
        std::cout<<"\n总用时: "<<t.TimerEnd().count()<<"ms"<<std::endl;
    }

    void printResult(const int operations,const int hits, const std::string& description)
    {
        const double hitRate = static_cast<double>(hits) / static_cast<double>(operations);
        std::cout<<description+" 热点数据访问测试结果:"<<std::endl;
        std::cout<<"操作数: "<<operations<<std::endl;
        std::cout<<"命中数: "<<hits<<std::endl;
        std::cout<<"命中率: "<<std::fixed<<std::setprecision(4)<<hitRate<<std::endl;
    }
}

int main()
{
    TEST::TestAlgorithm();
    return 0;
}