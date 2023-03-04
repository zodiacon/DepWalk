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
};

class CView : 
	public CFrameView<CView, IMainFrame>,
	public CVirtualListView<CView>,
	public CTreeViewHelper<CView> {
public:
	using CFrameView::CFrameView;

	bool ParseModules(PCWSTR path);
	HICON GetMainIcon() const;

	BOOL PreTranslateMessage(MSG* pMsg);

protected:
	BEGIN_MSG_MAP(CView)
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
	std::unique_ptr<ModuleInfo> ParsePE(PCWSTR name, HTREEITEM hParent, int icon = -1);

	LRESULT OnCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);

	CListViewCtrl m_ModuleList, m_ImportsList, m_ExportsList;
	CTreeViewCtrl m_Tree;
	CCustomHorSplitterWindow m_HSplitter, m_MainSplitter;
	CCustomSplitterWindow m_VSplitter;

	std::unordered_map<std::wstring, ModuleInfo*> m_ModulesMap;
	std::vector<std::unique_ptr<ModuleInfo>> m_Modules;
};
