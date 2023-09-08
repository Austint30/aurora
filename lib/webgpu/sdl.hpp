#include "wgpu.hpp"
#include <SDL.h>
#include <memory>

//
// Created by austin on 9/7/23.
//
namespace aurora::webgpu {
wgpu::Surface CreateSurfaceForWindow(const wgpu::Instance& instance, SDL_Window* window);
std::unique_ptr<wgpu::ChainedStruct> SetupWindowAndGetSurfaceDescriptor(SDL_Window* window);
} // namespace aurora::webgpu