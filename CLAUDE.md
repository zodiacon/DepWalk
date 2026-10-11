# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

DepWalk is a modern take on the classic *Dependency Walker* (`depends.exe`) tool for Windows — it opens a PE (EXE/DLL) file, recursively resolves its imported modules, and displays the resulting dependency tree along with each module's exports, imports, and header details. It is a native C++20 Win32 GUI app built with WTL (Windows Template Library), currently a work in progress.

## Build

Windows-only, Visual Studio 2022 required (PlatformToolset `v145`, C++20).

- Clone with `--recursive` (or run `git submodule update --init`) to pull the `wtlhelper` submodule — the build will fail without it.
- Use [vcpkg](https://github.com/microsoft/vcpkg) to install `detours`, `wtl`, and `wil` for `x64-windows` and/or `x86-windows`.
- Open `DepWalk.sln` in Visual Studio and build, or from a VS Developer prompt: `msbuild DepWalk.sln -p:Configuration=Release -p:Platform=x64` (use `x86` for 32-bit).
- Configurations: `Debug`, `Release`, `ReleaseSigned`. The solution's `ARM64` platform is just mapped to the x64 project configs, so it isn't a real ARM64 build.
- `ReleaseSigned` has a post-build `signtool sign /sha1 <thumbprint>` step that needs the author's code-signing certificate installed, so it fails on other machines. Use `Release` to check that a change builds.
- No test suite or lint/CI scripts exist in this repo.

## Solution structure

`DepWalk.sln` has three projects, built bottom-up:

1. **`wtlhelper/WTLHelper`** (`WTLHelper.vcxproj`, static lib, git submodule from `zodiacon/wtlhelper`) — a reusable WTL helper library (custom controls, tree/list view helpers, theming/dark-mode support, column management, splitters, tab views, etc.). Treat it as an external dependency: prefer using its existing helpers over writing new UI plumbing in `DepWalk`, and avoid modifying it from within this repo unless the change specifically belongs upstream.
2. **`PECore`** (`PECore.vcxproj`, static lib) — thin wrapper around the third-party `libpe` library (`libpe.h`/`libpe.cpp`, from jovibor/libpe, vendored in-tree) that parses PE32/PE32+ file structure (headers, sections, imports, exports, resources, relocations, debug info, TLS, load config, COM descriptor, etc.). `PEFile` (`PEFile.h`/`.cpp`) wraps `libpe::Ilibpe` (obtained via `libpe::Createlibpe()`) to open a file, map it, and expose raw/typed reads.
3. **`DepWalk`** (`DepWalk.vcxproj`, the executable) — the WTL GUI application.

## DepWalk app architecture

Standard WTL SDI-ish app with a tabbed MDI-like frame:

- `DepWalk.cpp` — `_tWinMain` entry point; initializes COM, common controls, loads `AppSettings` from the registry (`SOFTWARE\ScorpioSoftware\DepWalk`), calls `WTLHelper::InitDarkMode(...)` (from WTLHelper's dark-mode subclassing lib) with the persisted mode, then runs the WTL message loop against `CMainFrame`.
- `MainFrm.h/.cpp` (`CMainFrame`) — top-level frame (`CFrameWindowImpl` + `CAutoUpdateUI`), owns a `CNativeCustomTabView` (`m_view`) so multiple opened PE files each get their own tab/view. Implements `IMainFrame` (`Interfaces.h`), which `CView` reaches via `Frame()` (`CFrameView<CView, IMainFrame>`) without depending on `CMainFrame` directly. Right now its only method is `InitMenu(HMENU)`. It attaches menu icons through `WTLHelper::InitMenu`, using the command→icon table in `CMainFrame::InitMenu`. Call it on every context menu before `TrackPopupMenu` (see `CView::OnTreeRightClick`/`OnRightClickList`), and again on the main menu after a dark-mode switch.
- `WINDOW_MENU_POSITION` in `MainFrm.cpp` is the index of the top-level Window menu. Update it whenever you add or remove a top-level menu in `DepWalk.rc`.
- Commands that act on the current document (e.g. `ID_EDIT_COPY`) are handled in `CMainFrame`, which forwards them to the active tab's `CView` (`m_view.GetPageData(m_view.GetActivePage())`). `CView` handles the same ID too, for its own context menus. `CView::CopySelected` copies from whichever child control (tree or list) has focus.
- `AppSettings` (`AppSettings.h/.cpp`) persists app-level options (`DarkMode`, `AlwaysOnTop`, both bools; `Font`, a binary `LOGFONT`; `RecentFiles`, a multi-string) to the registry (`SOFTWARE\ScorpioSoftware\DepWalk`) via WTLHelper's `Settings` base class — this is where new persisted options belong; add a `SETTING(...)`/`DEF_SETTING(...)` pair per option (or `DEF_SETTING_MULTI(...)` alone for a string-list setting, no `SETTING(...)` line needed) and load/apply it in `CMainFrame::OnCreate`.
- Dark mode: the `&Options > &Dark Mode` menu item (`ID_OPTIONS_DARKMODE`) toggles the setting and posts `WM_UPDATE_DARKMODE` (`Interfaces.h`); `CMainFrame::OnUpdateDarkMode` calls `WTLHelper::SwitchToMode(...)` and propagates the change to child windows. Use `WTLHelper::SuspendHook()`/`ResumeHook()` around system common dialogs (see `OnFileOpen`) to avoid the dark-mode window hook interfering with them. The font dialog needs more than that, so open it with `WTLHelper::InvokeFontDialog(dlg)` (see `OnOptionsFont`). This is the same mechanism used by the sibling `ObjectExplorer` project — don't reintroduce the older `ThemeHelper`/`COwnerDrawnMenu`/`CCustomTabView` theming path that still exists in WTLHelper for legacy consumers; the two systems aren't meant to run at once.
- Font: one app-wide font is stored in the static `CView::s_Font`. `_tWinMain` sets it from the persisted setting via `CView::SetAppFont` before any window exists. `CMainFrame::ApplyFont` saves the setting, calls `SetAppFont`, and broadcasts `WM_UPDATE_FONT` (`Interfaces.h`); each `CView` then re-applies the font to its tree and lists. Reset Font stores an all-zero `LOGFONT`, and `OnOptionsFont` treats an empty face name as "not set" (it starts the dialog from the icon-title font). `SetAppFont` does the same, substituting the icon-title font so the default matches the controls' native font. New controls in a view should be added to `CView::ApplyFont`.
- Always on top: `&Options > &Always On Top` (`ID_OPTIONS_ALWAYSONTOP`) toggles `CMainFrame::SetAlwaysOnTop`, which just flips `HWND_TOPMOST`/`HWND_NOTOPMOST` and the menu check state; applied on startup from the persisted setting.
- Recent files: `RecentFilesManager` (`RecentFilesManager.h/.cpp`) is a small MRU list (dedupe + cap at 15, most-recent-first), separate from `AppSettings` — `CMainFrame::m_RecentFiles` is seeded from `AppSettings::RecentFiles()` in `OnCreate` and re-persisted after every successful open. `CMainFrame::OpenFile(path)` is the single path both `OnFileOpen` (after the file dialog) and `OnRecentFile` go through — it creates the tab/view and, only on success, updates the MRU list and calls `UpdateRecentFilesMenu()`. That method locates the `&Recent Files` popup in the `&File` menu by its literal text and rebuilds its items with IDs starting at `ATL_IDS_MRU_FILE`; `OnRecentFile` maps a clicked ID back to a path via `id - ATL_IDS_MRU_FILE`. This mirrors the pattern in the sibling `TotalPE` project — when adding to the File menu, keep the `&Recent Files` submenu text in sync between `DepWalk.rc` and the lookup in `UpdateRecentFilesMenu`.
- `View.h/.cpp` (`CView`) — the workhorse per-document view: `CFrameView` + `CVirtualListView` + `CTreeViewHelper` mixins from WTLHelper. Holds:
  - `m_Tree` — the module dependency tree (root = opened file, children = its imports, recursively).
  - `m_ModuleList` — virtual list of all discovered modules (flattened), backed by `m_Modules` (`vector<unique_ptr<ModuleInfo>>`) and deduplicated via `m_ModulesMap` (case-insensitive path/name → `ModuleInfo*`).
  - `m_ImportsList` / `m_ExportsList` — show imports/exports of whichever tree node is currently selected, via `m_TreeItems` (`HTREEITEM` → `ModuleTreeInfo`, which pairs a `ModuleInfo*` with that node's resolved `PEImportFunction` list).
  - `ParseModules()` → `ParsePE()` recursively opens each imported module (via `PEFile`/`libpe`), resolving load order/paths, and populates the tree + module map; `BuildExports()` extracts a module's export table once, cached on `ModuleInfo`.
  - API sets (`api-ms-*`/`ext-ms-*`) aren't opened as files. `ParsePE` marks them `IsApiSet` and fills `ApiSetHost` via `ResolveApiSetHost` (`ApiSets.h/.cpp`). That function reads the loader's API set schema (v6, Windows 10+) from `PEB::ApiSetMap` in DepWalk's own process. With no importer it returns the default host, which the module list shows because `ModuleInfo` is shared across importers. With an importer it applies the schema's per-importer exceptions; `ParsePE` stores that result per tree node in `ModuleTreeInfo::ApiSetHost` for the imports list. In both columns (text from `ApiSetHostText` in `View.cpp`), "(none)" means the schema lists no host, "(not found)" means the contract isn't in this machine's schema (shown with the missing-module error icon), and "(unsupported)" means the schema isn't v6 (e.g. Windows 7/8.x).
  - `ModuleInfo` lazily computes/caches derived display data (file time string, image base, arch, subsystem) since these come from parsing the PE header on demand.
- Virtual list columns are described via `ColumnType` enum tags + `GetColumnManager`/`GetColumnText`/`GetRowImage` (WTLHelper's column/virtual-list-view pattern), not per-column hardcoded switch statements in the message map.

When adding new PE data to display (e.g. a new data directory), the flow is: extend `libpe`/`PECore` extraction if not already exposed → surface it on `ModuleInfo`/`ModuleTreeInfo` in `View.h` → add a `ColumnType` entry and wire it into `GetColumnText`/`GetRowImage` in `View.cpp`.

## Precompiled headers

Each project uses a `pch.h`/`pch.cpp` PCH. `DepWalk/pch.h` sets `WINVER`/`_WIN32_WINNT` to `0x0601` and pulls in the ATL/WTL headers (`atlapp`, `atlframe`, `atlctrls`, etc.) — new WTL/ATL includes generally belong there, not in individual `.cpp` files.
