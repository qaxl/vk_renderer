# Zulk Vulkan Renderer in C

> [!WARNING]
> This is just my own program, which is only for my educational purposes. This code shouldn't be used in production but feel free to read and use the code as you wish (within the license).

This is just my testing project to prove my friend wrong. A Vulkan renderer written in plain C.

## Screenshots gallery

TODO.

## Building

### From source

Tutorial to build this is coming somewhere in the far future. In the mean time, feel free to use [Google's Search Engine](https://google.com) to figure out how to do it.

### From binaries

See the GitHub [releases tab](https://github.com/qaxl/vk_renderer/releases). Binaries are provided for `linux-x86_64` and `win64`.

#### Officially supported platforms

| Platform     | Status |
| ------------ | ------ |
| Linux        | ✅     |
| Windows (x64)| ✅     |
| Android      | 🚧     |
| iOS          | ❌     |
| macOS        | ❌     |

Apple platforms can't be supported as I (`qaxl`) don't own any apple devices. Android support is underway.

## Dependencies

*: vulkan sub-dependency;
**: vulkan sub-dependency, optional

| Library                                            | Purpose                                  |
| -------                                            | -------                                  |
| [Vulkan](https://www.vulkan.org/)                  | rendering a picture to the screen.       |
| [volk](https://github.com/zeux/volk)*              | loads vulkan functions dynamically.      |
| [VulkanSDK](https://www.lunarg.com/vulkan-sdk/)**  | used for validation layers               |
| [SDL3](https://github.com/libsdl-org/SDL)          | windowing systems under unix platforms   |

## Purpose

This is just really non-production ready testing for myself, only to educate myself how to use Vulkan.

## Legal

Feel free to view and use the code according to [Apache 2.0 license](https://www.tldrlegal.com/license/apache-license-2-0-apache-2-0). No copyright is held liable.
