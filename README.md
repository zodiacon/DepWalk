# DepWalk

A modern take on the classic Dependency Walker tool

(Very mucg a work in progress)

![](https://github.com/zodiacon/DepWalk/blob/master/depwalk.png)

## BUilding

* Clone the repo with `--recursive` to get the submodule (WTLHelper)
* Use ![vcpkg](https://github.com/microsoft/vcpkg) to install the following libraries: `detours`, `wtl`, `wil` for `x64-windows` and/or `x86-windows`.
* Open the solution with Visual Studio 2022 and build it.
