if (UNIX)
    set(VOLK_STATIC_DEFINES VK_USE_PLATFORM_WAYLAND_KHR)
endif()

add_subdirectory(volk)

set(SDL_STATIC ON)
set(SDL_STATIC_PIC OFF)

set(SDL_SHARED OFF)

add_subdirectory(SDL)
