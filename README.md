# Avalonia x Vulkan
Here is a simple proof of concept of rendering Vulkan frames on an Avalonia GUI.

## Motivation
Some time ago I was prototyping some game engine editor type projects. I eventually chose Avalonia because it's free, open source, cross-platform and retained mode. Unfortunately (at least at the time) I wasn't able to find any resources for rendering Vulkan framebuffers on an Avalonia canvas, so I made this prototype.

## Implementation explanation
<img align="right" width="350" height="334" src="https://github.com/user-attachments/assets/9f2eb69d-e383-4698-923f-e159680ba57a">
The solution consists of two components. The Vulkan renderer, which is a C++ library and the .NET frontend, which handles the GUI with Avalonia. The components communicate through an interop interface which allows the C# frontend to interact with the unmanaged backend.

The interface defines functions for creating and destroying the vulkan renderer, and also functions for attaching or detaching the render canvas in Avalonia. The Vulkan renderer is intialized lazily the first time the render canvas is attached. After this, the engine remains active in the background and the render canvas can be attached/detached at will. Standard output is propagated out of the unmanaged code.

Currently it's only for windows. The render canvas is built on top of the Win32 API.
<br>
<br>
## Usage
1. Dependencies
   * Vulkan 1.3.243.0 - should be located in C:\VulkanSDK\1.3.243.0\
   * GLM 1.0.1 - should be located in C:\GLM\
   * You can adjust the paths in the project settings.
2. Build and execute the AvaloniaGUI project
3. The UI should be green to start (indicates that no render surface is attached)
4. After clicking the button, vulkan will be initialized and attached to the UI
5. the button can be clicked again to toggle the viewport on or off.

## TODO
- [ ] Canvas resizing
- [ ] User input (camera movement)
- [ ] Multiple viewports
- [ ] Dynamic UI (docking)
- [ ] More platforms (linux and android would be nice)
