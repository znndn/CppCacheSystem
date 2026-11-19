cmake_minimum_required(VERSION 3.16)
project(CacheAlgorithm)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)

add_executable(CacheAlgorithm TestAlgorithm.cpp
        LFUAlgorithm.h
        LRUAlgorithm.h
        AlgorithmStandard.h
)

set_target_properties(CacheAlgorithm PROPERTIES CLEAN_DIRECT_OUTPUT 1)
