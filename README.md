# Free Camera Combat Group Fix

SKSE plugin for Skyrim Special Edition (SE 1.5.97 and Anniversary Edition runtimes). Fixes vanilla crashes in the
player's combat-group update that happen while the free camera is on (`tfc`, or a mod's photo / preview camera) and a
fight starts or ends nearby. On 1.6.1170 Crash Logger shows them as `SkyrimSE.exe+0CC90CF` (a read at `0x160`),
`SkyrimSE.exe+083D0CD`, or an access violation in `VCRUNTIME140.dll`'s memmove under `PlayerCharacter`.

Every frame the combat manager job (a worker thread) runs the player's combat-group upkeep: join or leave a combat
group, the time-in-group counter, the combat flags. While the camera is the free camera, Main also runs a reduced player
update of its own, and that update calls the same upkeep on the main thread. Neither caller takes a lock, so the two
threads can remove the player from one group at once (the member count wraps, or a copy reads past the end), or one
clears the player's group pointer right after the other stored it and reads it back.

The plugin replaces the main thread's call with a 5-byte nop. The free camera then works like every other camera: the
job's per-frame call stays, and at load the upkeep still runs through its own call there. The call is found by its
target inside the free-camera update (Address Library ids for both functions); if the code there is anything else, the
plugin patches nothing and says so in `FreeCamCombatGroupFix.log`. VR is not supported.

## Requirements
- [SKSE64](https://skse.silverlock.org/)
- [Address Library for SKSE Plugins](https://www.nexusmods.com/skyrimspecialedition/mods/32444)

## Building
Visual Studio 2022 (C++), CMake 3.24+, [vcpkg](https://github.com/microsoft/vcpkg) with `VCPKG_ROOT` set.

```
git clone --recurse-submodules https://github.com/Yuof/FreeCamCombatGroupFix
cd FreeCamCombatGroupFix
cmake --preset release
cmake --build --preset release
```

The DLL is `build/Release/FreeCamCombatGroupFix.dll`.

## License
[GPL-3.0-or-later](LICENSE), as required by [CommonLibSSE NG](https://github.com/alandtse/CommonLibVR) (GPL-3.0-or-later
with its modding exceptions), which this plugin links statically.
