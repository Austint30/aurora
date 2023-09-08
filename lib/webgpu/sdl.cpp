#include "sdl.hpp"
#include <SDL_syswm.h>

//
// Created by austin on 9/7/23.
//
namespace aurora::webgpu {
wgpu::Surface CreateSurfaceForWindow(const wgpu::Instance& instance, SDL_Window* window){
  std::unique_ptr<wgpu::ChainedStruct> const chainedDescriptor =
      SetupWindowAndGetSurfaceDescriptor(window);

  wgpu::SurfaceDescriptor descriptor;
  descriptor.nextInChain = chainedDescriptor.get();
  wgpu::Surface surface = instance.CreateSurface(&descriptor);

  return surface;
}

std::unique_ptr<wgpu::ChainedStruct> SetupWindowAndGetSurfaceDescriptor(SDL_Window* window){

  SDL_SysWMinfo wmInfo;
  SDL_VERSION(&wmInfo.version);
  SDL_GetWindowWMInfo(window, &wmInfo);

  switch (wmInfo.subsystem) {
  case SDL_SYSWM_X11:
  {
    std::unique_ptr<wgpu::SurfaceDescriptorFromXlibWindow> desc =
        std::make_unique<wgpu::SurfaceDescriptorFromXlibWindow>();
    desc->display = wmInfo.info.x11.display;
    desc->window = wmInfo.info.x11.window;
    return std::move(desc);
  }
  default:
    return;
  }

}
}