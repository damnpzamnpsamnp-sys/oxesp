cmake_minimum_required(VERSION 3.22)
project(oxesp LANGUAGES CXX)

add_library(oxesp SHARED esp.cpp)
target_compile_features(oxesp PRIVATE cxx_std_20)
target_compile_options(oxesp PRIVATE -O2 -fvisibility=hidden)
target_link_libraries(oxesp log android)
