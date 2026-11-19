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
    void printResult(int operations,int hits, const std::string& description,int LfuAgingCount);
    
    template <typename algorithm>
    void AlgorithmTest
        (algorithm& Algorithm,
        std::vector<std::pair<std::string,int>> &TestOperations,
        int operations,
        std::string MethodName);

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

    void UseAlgorithmTest()
    {
        int operations=0;
        std::vector<std::pair<std::string,int>> TestOperations;

        std::random_device seed;
        std::mt19937 rng(seed());

        // 30% 写入、70% 读取，每次访问有 70% 的概率选中热点键。
        std::uniform_int_distribution GetOrPut(1,10);
        std::uniform_int_distribution HotOrCold(1,10);
        std::uniform_int_distribution HotKeyGen(0,HOTKEY-1);
        std::uniform_int_distribution ColdKeyGen(HOTKEY, HOTKEY + COLDKEYS - 1);

        for (int i=0;i<OPERATIONS;i++)
        {
            int CurrentKey=-1;
            std::pair<std::string,int> curr;

            if (GetOrPut(rng)<=3) 
            {
                if (HotOrCold(rng)<=3)
                {
                    CurrentKey = ColdKeyGen(rng);
                }
                else
                {
                    CurrentKey = HotKeyGen(rng);
                }
                curr = {"PUT",CurrentKey};
            }
            else // 命中率的分母只计入 get 请求。
            {
                operations++;
                if (HotOrCold(rng)<=3)
                {
                    CurrentKey = ColdKeyGen(rng);
                }
                else
                {
                    CurrentKey = HotKeyGen(rng);
                }
                
                curr = {"GET",CurrentKey};
            }
            TestOperations.push_back(curr);
        }
        
        // 三种策略处理同一操作序列，两个 LFU 实例对照老化效果。
        LRU::LRUAlgorithm<int,std::string> lru;
        LFU::LFUAlgorithm<int,std::string> lfuNoReduction(INT_MAX);
        LFU::LFUAlgorithm<int,std::string> lfuWithReduction(100); 

        AlgorithmTest(lru,TestOperations,operations,"LRU算法");
        AlgorithmTest(lfuNoReduction,TestOperations,operations,"LFU无衰减算法");
        AlgorithmTest(lfuWithReduction,TestOperations,operations,"LFU有衰减算法");

        return;
    }

    template <typename algorithm>
    void AlgorithmTest
        (algorithm& Algorithm,
        std::vector<std::pair<std::string,int>> &TestOperations,
        int operations,
        std::string MethodName)
    {

        int hits=0;
        int LfuAgingCount=0;

        // 用同一批热点键为当前缓存预热。
        for (int i=0;i<HOTKEY;i++)
        {
            std::string ToBeValue = "value" + std::to_string(i);
            Algorithm.put(ToBeValue,i);
        }

        Timer t;
        t.TimerStart();

        for (auto i: TestOperations)
        {
            if (i.first=="PUT")
            {
                Algorithm.put("value"+std::to_string(i.second),i.second);
            }
            else
            {
                std::string retrived_value;

                if (Algorithm.get(i.second,retrived_value)==true)
                {
                    hits++;
                    if ((retrived_value!="value"+std::to_string(i.second)))
                    {
                        if (i.second<20)
                        {
                            std::cout<<MethodName<<" get 热数据时,读取错误在"<<retrived_value<<std::endl;
                        }
                        else
                        {
                            std::cout<<MethodName<<" get 冷数据时,读取错误在"<<retrived_value<<std::endl;
                        }
                    }
                }
            }
        }

        std::cout<<"\n总用时: "<<t.TimerEnd().count()<<"ms"<<std::endl;

        if (MethodName=="LFU有衰减算法")
        {
            LfuAgingCount=Algorithm.GetLfuAgingCount();
        }

        printResult(operations,hits,MethodName,LfuAgingCount);
    }

    void printResult(const int operations,const int hits, const std::string& description,int LfuAgingCount)
    {
        const double hitRate = static_cast<double>(hits) / static_cast<double>(operations);
        std::cout<<description+" 热点数据访问测试结果:"<<std::endl;
        std::cout<<"总操作次数: "<<OPERATIONS<<std::endl;
        std::cout<<"总读取次数: "<<operations<<std::endl;
        if (description=="LFU有衰减算法")
        {
            std::cout<<"衰减触发次数: "<<LfuAgingCount<<std::endl;
        }
        std::cout<<"命中数: "<<hits<<std::endl;
        std::cout<<"命中率: "<<std::fixed<<std::setprecision(4)<<hitRate<<std::endl;
    }
}

int main()
{
    TEST::UseAlgorithmTest();

    return 0;
}