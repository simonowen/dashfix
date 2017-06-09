# DashFix (OpenVR DashBoard Fixer) v1.1

## Introduction

Inputs from connected game controllers can interfere with OpenVR Dashboard
pointer navigation. The problem manifests itself as the inability to click
buttons and other elements in the dashboard user interface.

Steering wheels and HOTAS devices are usually the cause, as they often have a
rest position with a non-zero axis value, which the dashboard continually acts
upon. It's inconvenient to disconnect them during dashboard use, and there's
currently no way to tell Steam to ignore them.

DashFix lets you ignore the unwanted inputs, and use the dashboard normally.

## Install

- Connect any problematic game controllers
- Download and run the [installer for the latest version](https://github.com/simonowen/dashfix/releases).
- Select the controllers to block from the list
- Click OK

DashFix will continue running in the background, and start with Windows.

To change which controllers are blocked, re-launch DashFix from the Start
Menu shortcut. To deactivate it, uninstall from "Add or Remove Programs".

## Upgrade

To upgrade an earlier version simply over-install with the latest version.

## Internals

DashFix injects a DLL into Steam.exe and vrdashboard.exe, hooking calls to
SDL_GetJoystickAxis so they return zero for some controllers. This process
injection technique could upset some runtime virus scanners.

Source code is available from the [DashFix project page](https://github.com/simonowen/dashfix) on GitHub.

Please let me know if you have any problems, or find other places where
controllers are interfering with normal use.

## Changelog

### v1.1
- added installer/uninstaller to simplify use
- removed option to start with Windows as it's the default behaviour
- inverted checkboxes, so selected means blocked (thanks ljford7!)

### v1.0
- added individual controller selection
- added optional launch on Windows startup
- improved pre-hook checks and error handling
- added MIT license

### v0.1
- first public test release, blocking all controllers

---

Simon Owen  
https://github.com/simonowen/dashfix
