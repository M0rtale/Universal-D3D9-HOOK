# Universal-D3D9-HOOK
32-bit D3D9 hook

How to use:
- include D3D9 SDK
- include d3dx9.lib, detours.lib, psapi.lib into the linker additional dependancies and their corresponding location
- include d3dx9 and detour(included) header file location 

Benefits of using this hook:
- removes the need to do FindWindow, if the game hooks such function, it is not needed.
- removes the need to reverse the process to find the location where D3D9 device is declared.
- basically universal.

POC can be found here: https://www.unknowncheats.me/forum/c-and-c-/277517-finding-dx9-device-pointer-cf-guard-hooks.html

Credit: wlan aka jan
