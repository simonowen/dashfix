OpenVR Dashboard Fixer v0.1
---------------------------

Prevents inputs from connected game controllers from interfering with OpenVR
Dashboard navigation. This tends to be steering wheels and HOTAS devices, which
have a rest position with a non-zero axis value.

At present, directional input from ALL controllers is blocked. This could be
changed to a whitelist/blacklist in a future version.

The implementation injects a DLL into Steam.exe and vrdashboard.exe, to patch
calls to SDL_GetJoystickAxis so they always return zero. This process injection
technique could upset some runtime virus scanners.

Please let me know if you find any other places where controllers are causing
trouble. It may be as simple as adding extra modules or functions to the current
list. 

USAGE

Simply run the EXE, and it will sit silently in the background doing its work.
It doesn't matter if you start it before or after Steam, or if you restart Steam
while it's running.

Launching the EXE a second time will prompt to close the running instance,
however the code patches will remain in place until Steam/SteamVR is restarted.

---

Simon Owen
https://github.com/simonowen/dashfix
