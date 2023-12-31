cmake_minimum_required(VERSION 3.15)
project(CRenderer)

add_compile_options(-fsanitize=address)
add_link_options(-fsanitize=address)

add_subdirectory(src)
add_subdirectory(third_party)
