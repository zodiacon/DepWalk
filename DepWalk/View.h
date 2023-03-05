#pragma once

#include "Interfaces.h"
#include <FrameView.h>
#include <VirtualListView.h>
#include <TreeViewHelper.h>
#include <CustomSplitterWindow.h>
#include <PEFile.h>

struct ModuleInfo {
	PEFile PE;
	std::wstring FullPath;
	std::wstring Name;
	std::vector<libpe::PEExportFunction> Exports;
	int Icon;
	bool IsApiSet;

	CString const& GetFileTime() const;

private:
	mutable CString FileTime;
};

struct ModuleTreeInfo {
	std::vector<libpe::PEImportFunction> Imports;
	ModuleInfo* Module{ nullptr };
};

class CView : 
	public CFrameView<CView, IMainFrame>,
	public CVirtualListView<CView>,
	public CTreeViewHelper<CView> {
public:
	using CFrameView::CFrameView;

	bool ParseModules(PCWSTR path);
	HICON GetMainIcon() const;
	CString GetColumnText(HWND h, int row, int col) const;
	int GetRowImage(HWND h, int row, int col) const;
	void OnTreeSelChanged(HWND tree, HTREEITEM hOld, HTREEITEM hNew);
	void DoSort(SortInfo const* si);

	BOOL PreTranslateMessage(MSG* pMsg);
	static CString UndecorateName(PCSTR name);

protected:
	BEGIN_MSG_MAP(CView)
		MESSAGE_HANDLER(WM_SETFOCUS, OnSetFocus)
		MESSAGE_HANDLER(WM_CREATE, OnCreate)
		CHAIN_MSG_MAP(BaseFrame)
		CHAIN_MSG_MAP(CVirtualListView<CView>)
		CHAIN_MSG_MAP(CTreeViewHelper<CView>)
	END_MSG_MAP()

	// Handler prototypes (uncomment arguments if needed):
	//	LRESULT MessageHandler(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
	//	LRESULT CommandHandler(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	//	LRESULT NotifyHandler(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/)

private:
	enum class ColumnType {
		Name, Path, FileTime, LinkTime, FileSize, LinkChecksum, Arch,
		Hint, Ordinal, UndecoratedName, ForwardedName, RVA, NameRVA,
	};

	std::pair<HTREEITEM, ModuleInfo*> ParsePE(PCWSTR name, HTREEITEM hParent, int icon = -1);
	void BuildExports(ModuleInfo* mi, libpe::PEExport* exports) const;

	LRESULT OnCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnSetFocus(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);

	CListViewCtrl m_ModuleList, m_ImportsList, m_ExportsList;
	CTreeViewCtrl m_Tree;
	CCustomHorSplitterWindow m_HSplitter, m_MainSplitter;
	CCustomSplitterWindow m_VSplitter;

	struct Compare {
		bool operator()(std::wstring const& s1, std::wstring const& s2) const {
			return _wcsicmp(s1.c_str(), s2.c_str()) < 0;
		}
	};

	std::map<std::wstring, ModuleInfo*, Compare> m_ModulesMap;
	std::vector<std::unique_ptr<ModuleInfo>> m_Modules;
	std::unordered_map<HTREEITEM, std::unique_ptr<ModuleTreeInfo>> m_TreeItems;
};
