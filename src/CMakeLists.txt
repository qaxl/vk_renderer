add_executable(renderer
    io.c main.c renderer.c types.c surface.c)

if (SDL_WAYLAND)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(GTK REQUIRED gtk+-3.0)

    target_link_directories(renderer PUBLIC ${GTK_LIBRARY_DIRS})
    # target_compile_options()(renderer PUBLIC ${GTK_CFLAGS_OTHER})
    target_include_directories(renderer PUBLIC ${GTK_INCLUDE_DIRS})
else()
    set(GTK_LIBRARIES )
endif()

target_link_libraries(renderer PRIVATE volk SDL3::SDL3 ${GTK_LIBRARIES})
