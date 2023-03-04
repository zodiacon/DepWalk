// View.cpp : implementation of the CView class
//
/////////////////////////////////////////////////////////////////////////////

#include "pch.h"
#include "resource.h"
#include "View.h"
#include <SortHelper.h>

BOOL CView::PreTranslateMessage(MSG* pMsg) {
	pMsg;
	return FALSE;
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
	ParsePE(path, TVI_ROOT, image);

	m_Tree.Expand(m_Tree.GetRootItem(), TVE_EXPAND);
	m_Tree.SetRedraw(TRUE);

	m_ModuleList.SetItemCount((int)m_Modules.size());

	return true;
}

HICON CView::GetMainIcon() const {
	return m_Tree.GetImageList().GetIcon(m_Tree.GetImageList().GetImageCount() - 1);
}

std::unique_ptr<ModuleInfo> CView::ParsePE(PCWSTR name, HTREEITEM hParent, int icon) {
	auto apiSet = _wcsnicmp(name, L"api-ms-", 7) == 0;
	auto fullpath = apiSet ? false : wcschr(name, L'\\') != nullptr;
	auto mi = std::make_unique<ModuleInfo>();
	if (!fullpath)
		mi->Name = name;
	auto hLib = apiSet ? nullptr : ::LoadLibraryEx(name, nullptr, LOAD_LIBRARY_AS_DATAFILE);
	if(icon < 0)
		icon = apiSet ? 1 : 0;
	if (hLib) {
		WCHAR path[MAX_PATH];
		DWORD chars = 1;
		if (!fullpath) {
			chars = ::GetModuleFileName(hLib, path, _countof(path));
			::FreeLibrary(hLib);
			if (chars)
				name = path;
		}
		mi->FullPath = name;
		if (mi->Name.empty())
			mi->Name = wcsrchr(name, L'\\') + 1;
		auto hItem = m_Tree.InsertItem(mi->Name.c_str(), icon, icon, hParent, TVI_LAST);
		if ((chars && mi->PE.Open(name))) {
			auto imports = mi->PE->GetImport();
			if (imports) {
				for (auto& lib : *imports) {
					std::wstring libname = (PCWSTR)CString(lib.ModuleName.c_str());
					ParsePE(libname.c_str(), hItem);
				}
			}
		}
	}
	else {
		auto hItem = m_Tree.InsertItem(mi->Name.c_str(), icon, icon, hParent, TVI_LAST);
	}
	if (!m_ModulesMap.contains(name)) {
		m_ModulesMap.insert({ name, mi.get() });
		m_Modules.push_back(std::move(mi));
	}
	return mi;
}

LRESULT CView::OnCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/) {
	m_hWndClient = m_MainSplitter.Create(m_hWnd, rcDefault, nullptr, WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN);
	m_VSplitter.Create(m_MainSplitter, rcDefault, nullptr, WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN);
	m_HSplitter.Create(m_VSplitter, rcDefault, nullptr, WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN);

	m_ModuleList.Create(m_MainSplitter, rcDefault, nullptr, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS |
		LVS_OWNERDATA | LVS_REPORT);
	m_ModuleList.SetExtendedListViewStyle(LVS_EX_DOUBLEBUFFER | LVS_EX_INFOTIP | LVS_EX_FULLROWSELECT);

	m_Tree.Create(m_VSplitter, rcDefault, nullptr, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | TVS_HASBUTTONS | TVS_HASLINES | TVS_LINESATROOT);
	m_Tree.SetExtendedStyle(TVS_EX_DOUBLEBUFFER, TVS_EX_DOUBLEBUFFER);

	m_MainSplitter.SetSplitterPanes(m_VSplitter, m_ModuleList);
	m_MainSplitter.SetSplitterPosPct(60);
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

	UINT icons[] = { IDI_DLL, IDI_INTERFACE };
	CImageList images;
	images.Create(16, 16, ILC_COLOR32 | ILC_MASK, 4, 4);
	for (auto icon : icons)
		images.AddIcon(AtlLoadIconImage(icon, 0, 16, 16));
	m_Tree.SetImageList(images);

	return 0;
}
