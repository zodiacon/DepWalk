// View.cpp : implementation of the CView class
//
/////////////////////////////////////////////////////////////////////////////

#include "pch.h"
#include "resource.h"
#include "View.h"
#include <SortHelper.h>
#include <DbgHelp.h>

#pragma comment(lib, "dbghelp")

BOOL CView::PreTranslateMessage(MSG* pMsg) {
	pMsg;
	return FALSE;
}

LRESULT CView::OnSetFocus(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/) {
	m_Tree.SetFocus();
	return 0;
}

bool CView::ParseModules(PCWSTR path) {
	auto mi = std::make_unique<ModuleInfo>();
	auto& pe = mi->PE;
	if (!pe.Open(path))
		return false;

	m_Modules.reserve(64);
	m_Tree.DeleteAllItems();
	m_Tree.SetRedraw(FALSE);
	WCHAR fullpath[MAX_PATH];
	wcscpy_s(fullpath, path);
	WORD icon = 0;
	auto hIcon = ::ExtractAssociatedIcon(_Module.GetModuleInstance(), fullpath, &icon);
	int image = -1;
	if (hIcon)
		image = m_Tree.GetImageList(TVSIL_NORMAL).AddIcon(hIcon);
	auto [hItem, m] = ParsePE(path, TVI_ROOT, image);
	auto tmi = std::make_unique<ModuleTreeInfo>();
	tmi->Module = m;
	m_TreeItems.insert({ hItem, std::move(tmi) });
	m_Tree.Expand(m_Tree.GetRootItem(), TVE_EXPAND);
	m_Tree.SelectItem(hItem);

	m_Tree.SetRedraw(TRUE);

	m_ModuleList.SetItemCount((int)m_Modules.size());

	return true;
}

HICON CView::GetMainIcon() const {
	return m_Tree.GetImageList().GetIcon(m_Tree.GetImageList().GetImageCount() - 1);
}

CString CView::GetColumnText(HWND h, int row, int col) const {
	auto tag = GetColumnManager(h)->GetColumnTag<ColumnType>(col);
	if (h == m_ModuleList) {
		auto& mi = m_Modules[row];
		switch (tag) {
			case ColumnType::Name: return mi->Name.c_str();
			case ColumnType::Path: return mi->FullPath.c_str();
			case ColumnType::FileSize:
				if (!mi->IsApiSet && mi->PE->IsLoaded()) {
					WCHAR text[64];
					::StrFormatByteSize(mi->PE.GetFileSize(), text, _countof(text));
					return text;
				}
				break;
			case ColumnType::FileTime:
				return mi->FullPath.empty() ? L"" : (PCWSTR)mi->GetFileTime();

		}
	}
	else if (h == m_ExportsList) {
		auto const& mod = m_TreeItems.at(m_Tree.GetSelectedItem());
		auto mi = mod->Module;
		ATLASSERT(mi);
		auto& exp = mi->Exports[row];
		switch (tag) {
			case ColumnType::Name: return exp.FuncName.c_str();
			case ColumnType::ForwardedName: return exp.ForwarderName.c_str();
			case ColumnType::Ordinal: return std::to_wstring(exp.Ordinal).c_str();
			case ColumnType::RVA: return std::format(L"0x{:X}", exp.FuncRVA).c_str();
			case ColumnType::NameRVA: return std::format(L"0x{:X}", exp.NameRVA).c_str();
			case ColumnType::UndecoratedName: return exp.FuncName.empty() ? L"" : (PCWSTR)UndecorateName(exp.FuncName.c_str());
		}
	}
	else {
		auto const& mod = m_TreeItems.at(m_Tree.GetSelectedItem());
		auto const& func = mod->Imports[row];
		switch (tag) {
			case ColumnType::Name: return func.FuncName.c_str();
			case ColumnType::Hint: return std::to_wstring(func.ImpByName.Hint).c_str();
			case ColumnType::Ordinal: return func.ImpByName.Name[0] == 0 ? std::to_wstring(func.unThunk.Thunk32.u1.Ordinal).c_str() : L"0";
			case ColumnType::UndecoratedName: return func.FuncName.empty() ? L"" : (PCWSTR)UndecorateName(func.FuncName.c_str());
		}
	}

	return CString();
}

CString CView::UndecorateName(PCSTR name) {
	CHAR result[2048];
	return 0 == ::UnDecorateSymbolName(name, result, _countof(result), 0) ? "" : result;
}

int CView::GetRowImage(HWND h, int row, int col) const {
	if (h == m_ModuleList) {
		auto& mi = m_Modules[row];
		return mi->Icon;
	}
	else if (h == m_ExportsList) {
		return m_TreeItems.at(m_Tree.GetSelectedItem())->Module->Exports[row].ForwarderName.empty() ? 1 : 0;
	}

	return 0;
}

void CView::OnTreeSelChanged(HWND tree, HTREEITEM hOld, HTREEITEM hNew) {
	ATLASSERT(m_Tree.GetSelectedItem() == hNew);
	if (auto it = m_TreeItems.find(hNew); it != m_TreeItems.end()) {
		m_ImportsList.SetItemCount((int)it->second->Imports.size());
		auto mi = it->second->Module;
		m_ExportsList.SetItemCount(mi ? (int)mi->Exports.size() : 0);
	}
	else {
		m_ImportsList.SetItemCount(0);
		m_ExportsList.SetItemCount(0);
	}
}

void CView::DoSort(SortInfo const* si) {
	auto asc = si->SortAscending;
	auto tag = GetColumnManager(si->hWnd)->GetColumnTag<ColumnType>(si->SortColumn);

	if (si->hWnd == m_ModuleList) {
		auto compare = [&](auto& m1, auto& m2) {
			switch (tag) {
				case ColumnType::Name: return SortHelper::Sort(m1->Name, m2->Name, asc);
				case ColumnType::Path: return SortHelper::Sort(m1->FullPath, m2->FullPath, asc);
				case ColumnType::FileTime: return SortHelper::Sort(m1->FileTime, m2->FileTime, asc);
				case ColumnType::FileSize: return SortHelper::Sort(m1->PE.GetFileSize(), m2->PE.GetFileSize(), asc);
			}
			return false;
		};
		std::ranges::sort(m_Modules, compare);
	}
}

std::pair<HTREEITEM, ModuleInfo*> CView::ParsePE(PCWSTR name, HTREEITEM hParent, int icon) {
	auto apiSet = _wcsnicmp(name, L"api-ms-", 7) == 0 || _wcsnicmp(name, L"ext-ms-", 7) == 0;
	auto fullpath = apiSet ? false : wcschr(name, L'\\') != nullptr;
	auto mi = std::make_unique<ModuleInfo>();
	auto m = mi.get();
	bool newModule = false;

	if (fullpath) {
		if (auto it = m_ModulesMap.find(name); it != m_ModulesMap.end())
			m = it->second;
	}
	else {
		m->Name = name;
	}

	auto hLib = apiSet || fullpath ? nullptr : ::LoadLibraryEx(name, nullptr, DONT_RESOLVE_DLL_REFERENCES);
	auto ext = wcsrchr(name, L'.');
	if (!apiSet && !fullpath && hLib == nullptr && _wcsicmp(ext, L".sys") == 0) {
		//
		// try in drivers directory for sys files
		//
		static std::wstring driversDir;
		if (driversDir.empty()) {
			WCHAR path[MAX_PATH];
			::GetSystemDirectory(path, _countof(path));
			wcscat_s(path, L"\\Drivers\\");
			driversDir = path;
		}
		hLib = ::LoadLibraryEx((driversDir + name).c_str(), nullptr, DONT_RESOLVE_DLL_REFERENCES);
		if (hLib)
			m->FullPath = driversDir + name;
	}

	if (icon < 0) {
		//
		// icon setting
		//
		icon = apiSet ? 1 : 0;
		if (icon == 0) {
			if (_wcsicmp(ext, L".sys") == 0)
				icon = 3;
		}
	}
	m->IsApiSet = apiSet;
	if (apiSet || fullpath) {
		if (auto it = m_ModulesMap.find(name); it != m_ModulesMap.end())
			m = it->second;
		else {
			newModule = true;
			m_ModulesMap.insert({ name, m });
		}
	}
	HTREEITEM hItem{ nullptr };
	if (hLib || fullpath) {
		WCHAR path[MAX_PATH];
		DWORD chars = 1;
		if (!fullpath) {
			chars = ::GetModuleFileName(hLib, path, _countof(path));
			if (hLib)
				::FreeLibrary(hLib);
			if (chars)
				name = path;
		}
		if (auto it = m_ModulesMap.find(name); it != m_ModulesMap.end()) {
			m = it->second;
		}
		else {
			m_ModulesMap.insert({ name, m });
			newModule = true;
		}

		m->FullPath = name;
		if (m->Name.empty())
			m->Name = wcsrchr(name, L'\\') + 1;
		hItem = m_Tree.InsertItem(m->Name.c_str(), icon, icon, hParent, TVI_LAST);
		if ((chars && !m->PE->IsLoaded() && m->PE.Open(name))) {
			auto imports = m->PE->GetImport();
			if (imports) {
				for (auto& lib : *imports) {
					std::wstring libname = (PCWSTR)CString(lib.ModuleName.c_str());
					auto [hSubItem, m2] = ParsePE(libname.c_str(), hItem);
					auto nodeImports = std::make_unique<ModuleTreeInfo>();
					nodeImports->Imports = lib.ImportFunc;
					nodeImports->Module = m2;
					m_TreeItems.insert({ hSubItem, std::move(nodeImports) });
				}
			}
		}
	}
	else {
		if (apiSet) {
			// TODO
		}
		if (!apiSet && m->FullPath.empty())
			icon = 2;
		hItem = m_Tree.InsertItem(m->Name.c_str(), icon, icon, hParent, TVI_LAST);
	}
	ATLASSERT(hItem);
	m->Icon = icon;
	if (m->PE->IsLoaded() && m->Exports.empty()) {
		auto exports = m->PE->GetExport();
		if (exports)
			BuildExports(m, exports);
	}
	if (newModule) {
		m_Modules.push_back(std::move(mi));
	}
	return { hItem, m };
}

void CView::BuildExports(ModuleInfo* mi, libpe::PEExport* exports) const {
	mi->Exports = exports->Funcs;
}

LRESULT CView::OnCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/) {
	m_hWndClient = m_MainSplitter.Create(m_hWnd, rcDefault, nullptr, WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN);
	m_VSplitter.Create(m_MainSplitter, rcDefault, nullptr, WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN);
	m_HSplitter.Create(m_VSplitter, rcDefault, nullptr, WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN);

	m_ModuleList.Create(m_MainSplitter, rcDefault, nullptr, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS |
		LVS_OWNERDATA | LVS_REPORT | LVS_SHAREIMAGELISTS);
	m_ModuleList.SetExtendedListViewStyle(LVS_EX_DOUBLEBUFFER | LVS_EX_INFOTIP | LVS_EX_FULLROWSELECT);

	m_Tree.Create(m_VSplitter, rcDefault, nullptr, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | TVS_HASBUTTONS | TVS_HASLINES | TVS_LINESATROOT | TVS_SHOWSELALWAYS);
	m_Tree.SetExtendedStyle(TVS_EX_DOUBLEBUFFER, TVS_EX_DOUBLEBUFFER);

	m_MainSplitter.SetSplitterPanes(m_VSplitter, m_ModuleList);
	m_MainSplitter.SetSplitterPosPct(65);
	m_VSplitter.SetSplitterPanes(m_Tree, m_HSplitter);
	m_VSplitter.SetSplitterPosPct(30);
	m_HSplitter.SetSplitterPosPct(50);

	m_ImportsList.Create(m_HSplitter, rcDefault, nullptr, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS |
		LVS_OWNERDATA | LVS_REPORT);
	m_ImportsList.SetExtendedListViewStyle(LVS_EX_DOUBLEBUFFER | LVS_EX_INFOTIP | LVS_EX_FULLROWSELECT);
	m_ExportsList.Create(m_HSplitter, rcDefault, nullptr, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS |
		LVS_OWNERDATA | LVS_REPORT);
	m_ExportsList.SetExtendedListViewStyle(LVS_EX_DOUBLEBUFFER | LVS_EX_INFOTIP | LVS_EX_FULLROWSELECT);
	m_HSplitter.SetSplitterPanes(m_ImportsList, m_ExportsList);

	{
		UINT icons[] = { IDI_DLL, IDI_INTERFACE, IDI_MOD_ERROR, IDI_SYSFILE };
		CImageList images;
		images.Create(16, 16, ILC_COLOR32 | ILC_MASK, 4, 4);
		for (auto icon : icons)
			images.AddIcon(AtlLoadIconImage(icon, 0, 16, 16));
		m_Tree.SetImageList(images);
		m_ModuleList.SetImageList(images, LVSIL_SMALL);
	}
	{
		UINT icons[] = { IDI_FUNC_EXPORT, IDI_FUNC };
		CImageList images;
		images.Create(16, 16, ILC_COLOR32 | ILC_MASK, 4, 4);
		for (auto icon : icons)
			images.AddIcon(AtlLoadIconImage(icon, 0, 16, 16));
		m_ExportsList.SetImageList(images, LVSIL_SMALL);
	}
	{
		UINT icons[] = { IDI_FUNC_IMPORT };
		CImageList images;
		images.Create(16, 16, ILC_COLOR32 | ILC_MASK, 4, 4);
		for (auto icon : icons)
			images.AddIcon(AtlLoadIconImage(icon, 0, 16, 16));
		m_ImportsList.SetImageList(images, LVSIL_SMALL);
	}

	auto cm = GetColumnManager(m_ModuleList);
	cm->AddColumn(L"Module Name", LVCFMT_LEFT, 280, ColumnType::Name);
	cm->AddColumn(L"Full Path", LVCFMT_LEFT, 350, ColumnType::Path);
	cm->AddColumn(L"File Size", LVCFMT_RIGHT, 100, ColumnType::FileSize);
	cm->AddColumn(L"File Time", LVCFMT_LEFT, 150, ColumnType::FileTime);

	cm = GetColumnManager(m_ImportsList);
	cm->AddColumn(L"Name", LVCFMT_LEFT, 250, ColumnType::Name);
	cm->AddColumn(L"Ordinal", LVCFMT_RIGHT, 70, ColumnType::Ordinal);
	cm->AddColumn(L"Hint", LVCFMT_RIGHT, 70, ColumnType::Hint);
	cm->AddColumn(L"Undecorated Name", LVCFMT_LEFT, 250, ColumnType::UndecoratedName);

	cm = GetColumnManager(m_ExportsList);
	cm->AddColumn(L"Name", LVCFMT_LEFT, 250, ColumnType::Name);
	cm->AddColumn(L"Forwarder Name", LVCFMT_LEFT, 250, ColumnType::ForwardedName);
	cm->AddColumn(L"Ordinal", LVCFMT_RIGHT, 70, ColumnType::Ordinal);
	cm->AddColumn(L"Function RVA", LVCFMT_RIGHT, 100, ColumnType::RVA);
	cm->AddColumn(L"Name RVA", LVCFMT_RIGHT, 100, ColumnType::NameRVA);
	cm->AddColumn(L"Undecorated Name", LVCFMT_LEFT, 250, ColumnType::UndecoratedName);

	return 0;
}

CString const& ModuleInfo::GetFileTime() const {
	if (!FullPath.empty() && m_FileTimeAsString.IsEmpty()) {
		auto hFile = ::CreateFile(FullPath.c_str(), FILE_READ_ATTRIBUTES, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
		if (hFile != INVALID_HANDLE_VALUE) {
			FILETIME dummy;
			::GetFileTime(hFile, &dummy, &dummy, (FILETIME*)&FileTime);
			WCHAR ft[96];
			DWORD flags = FDTF_SHORTDATE | FDTF_SHORTTIME | FDTF_NOAUTOREADINGORDER;
			if (::SHFormatDateTime((FILETIME*)&FileTime, &flags, ft, _countof(ft)))
				m_FileTimeAsString = ft;
			::CloseHandle(hFile);
		}
	}
	return m_FileTimeAsString;
}
