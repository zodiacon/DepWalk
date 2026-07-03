#include "pch.h"
#include "resource.h"
#include "AboutDlg.h"
#include "View.h"
#include "MainFrm.h"
#include "AppSettings.h"
#include <ToolbarHelper.h>
#include <thread>

const int WINDOW_MENU_POSITION = 5;

BOOL CMainFrame::PreTranslateMessage(MSG* pMsg) {
	if (CFrameWindowImpl<CMainFrame>::PreTranslateMessage(pMsg))
		return TRUE;

	return m_view.PreTranslateMessage(pMsg);
}

BOOL CMainFrame::OnIdle() {
	UIUpdateToolBar();
	return FALSE;
}

LRESULT CMainFrame::OnCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/) {
	ToolBarButtonInfo const buttons[] = {
		{ ID_FILE_OPEN, IDI_OPEN },
		{ 0 },
		{ ID_EDIT_COPY, IDI_COPY },
	};
	CreateSimpleReBar(ATL_SIMPLE_REBAR_NOBORDER_STYLE);
	auto tb = ToolbarHelper::CreateAndInitToolBar(m_hWnd, buttons, _countof(buttons));

	AddSimpleReBarBand(tb);
	UIAddToolBar(tb);

	CreateSimpleStatusBar();

	m_view.m_bTabCloseButton = FALSE;
	m_hWndClient = m_view.Create(m_hWnd, rcDefault, nullptr,
		WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN);
	CImageList images;
	images.Create(16, 16, ILC_COLOR32 | ILC_MASK, 4, 4);
	m_view.SetImageList(images);

	UISetCheck(ID_VIEW_TOOLBAR, 1);
	UISetCheck(ID_VIEW_STATUS_BAR, 1);

	// register object for message filtering and idle updates
	CMessageLoop* pLoop = _Module.GetMessageLoop();
	ATLASSERT(pLoop != NULL);
	pLoop->AddMessageFilter(this);
	pLoop->AddIdleHandler(this);

	CMenuHandle menuMain = GetMenu();
	m_view.SetWindowMenu(menuMain.GetSubMenu(WINDOW_MENU_POSITION));

	if (sizeof(PVOID) == 8) {
		CString text;
		GetWindowText(text);
		text += L" (64 bit)";
		SetWindowText(text);
	}

	InitMenu(GetMenu());
	UIAddMenu(GetMenu());

	if (AppSettings::Get().DarkMode())
		UISetCheck(ID_OPTIONS_DARKMODE, true);

	SetAlwaysOnTop(AppSettings::Get().AlwaysOnTop());

	m_RecentFiles.Set(AppSettings::Get().RecentFiles());
	UpdateRecentFilesMenu();

	return 0;
}

void CMainFrame::InitMenu(HMENU hMenu) {
	MenuItemData const commands[] = {
		{ ID_EDIT_COPY, IDI_COPY },
		{ ID_FILE_OPEN, IDI_OPEN },
		{ ID_FILE_SAVE, IDI_SAVE },
		{ ID_WINDOW_CLOSE, IDI_WIN_CLOSE },
		{ ID_WINDOW_CLOSE_ALL, IDI_WIN_CLOSEALL },
	};

	WTLHelper::InitMenu(hMenu, commands, _countof(commands));
}

void CMainFrame::SetAlwaysOnTop(bool alwaysOnTop) {
	SetWindowPos(alwaysOnTop ? HWND_TOPMOST : HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE);
	UISetCheck(ID_OPTIONS_ALWAYSONTOP, alwaysOnTop);
}

LRESULT CMainFrame::OnDestroy(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled) {
	AppSettings::Get().Save();

	// unregister message filtering and idle updates
	CMessageLoop* pLoop = _Module.GetMessageLoop();
	ATLASSERT(pLoop != NULL);
	pLoop->RemoveMessageFilter(this);
	pLoop->RemoveIdleHandler(this);

	bHandled = FALSE;
	return 1;
}

LRESULT CMainFrame::OnFileExit(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/) {
	PostMessage(WM_CLOSE);
	return 0;
}

LRESULT CMainFrame::OnFileOpen(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/) {
	CSimpleFileDialog dlg(TRUE, nullptr, nullptr, OFN_EXPLORER | OFN_ENABLESIZING,
		L"PE Files\0*.exe;*.dll;*.ocx;*.efi\0All Files\0*.*\0", m_hWnd);
	WTLHelper::SuspendHook();
	auto ok = dlg.DoModal() == IDOK;
	WTLHelper::ResumeHook();
	if (ok)
		OpenFile(dlg.m_szFileName);

	return 0;
}

bool CMainFrame::OpenFile(PCWSTR path) {
	auto pView = new CView(this);
	pView->Create(m_view, rcDefault, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN, 0);
	CWaitCursor wait;
	if (!pView->ParseModules(path)) {
		pView->DestroyWindow();
		return false;
	}
	auto hIcon = pView->GetMainIcon();
	int index = CImageList(m_view.GetImageList()).AddIcon(hIcon);
	CString title(path);
	title = title.Mid(title.ReverseFind(L'\\') + 1);
	m_view.AddPage(pView->m_hWnd, title, index, pView);

	m_RecentFiles.AddFile(path);
	AppSettings::Get().RecentFiles(m_RecentFiles.Files());
	UpdateRecentFilesMenu();

	return true;
}

void CMainFrame::UpdateRecentFilesMenu() {
	if (m_RecentFiles.IsEmpty())
		return;

	auto menu = CMenuHandle(GetMenu()).GetSubMenu(0);
	CString text;
	int i = 0;
	for (; ; i++) {
		menu.GetMenuString(i, text.GetBuffer(128), 128, MF_BYPOSITION);
		text.ReleaseBuffer();
		if (text == L"&Recent Files")
			break;
	}
	menu = menu.GetSubMenu(i);
	while (menu.DeleteMenu(0, MF_BYPOSITION))
		;

	i = 0;
	for (auto& file : m_RecentFiles.Files())
		menu.AppendMenu(MF_BYPOSITION, ATL_IDS_MRU_FILE + i++, file.c_str());
}

LRESULT CMainFrame::OnRecentFile(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/) {
	auto& path = m_RecentFiles.Files()[wID - ATL_IDS_MRU_FILE];
	OpenFile(path.c_str());
	return 0;
}

LRESULT CMainFrame::OnViewToolBar(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/) {
	static bool bVisible = TRUE;	// initially visible
	bVisible = !bVisible;
	CReBarCtrl rebar = m_hWndToolBar;
	int nBandIndex = rebar.IdToIndex(ATL_IDW_BAND_FIRST + 1);	// toolbar is 2nd added band
	rebar.ShowBand(nBandIndex, bVisible);
	UISetCheck(ID_VIEW_TOOLBAR, bVisible);
	UpdateLayout();
	return 0;
}

LRESULT CMainFrame::OnViewStatusBar(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/) {
	auto bVisible = !::IsWindowVisible(m_hWndStatusBar);
	::ShowWindow(m_hWndStatusBar, bVisible ? SW_SHOWNOACTIVATE : SW_HIDE);
	UISetCheck(ID_VIEW_STATUS_BAR, bVisible);
	UpdateLayout();
	return 0;
}

LRESULT CMainFrame::OnAppAbout(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/) {
	CAboutDlg dlg;
	dlg.DoModal();
	return 0;
}

LRESULT CMainFrame::OnAboutWindows(WORD, WORD, HWND, BOOL&) {
	std::thread([this]() { ::ShellAbout(m_hWnd, L"Windows", nullptr, nullptr); }).detach();
	return 0;
}

LRESULT CMainFrame::OnWindowClose(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/) {
	int nActivePage = m_view.GetActivePage();
	if (nActivePage != -1)
		m_view.RemovePage(nActivePage);
	else
		::MessageBeep((UINT)-1);

	return 0;
}

LRESULT CMainFrame::OnWindowCloseAll(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/) {
	m_view.RemoveAllPages();

	return 0;
}

LRESULT CMainFrame::OnWindowActivate(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/) {
	int nPage = wID - ID_WINDOW_TABFIRST;
	m_view.SetActivePage(nPage);

	return 0;
}

LRESULT CMainFrame::OnToggleDarkMode(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/) {
	auto& settings = AppSettings::Get();
	settings.DarkMode(!settings.DarkMode());
	PostMessage(WM_UPDATE_DARKMODE, 0, 0);
	return 0;
}

LRESULT CMainFrame::OnUpdateDarkMode(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/) {
	auto& settings = AppSettings::Get();

	WTLHelper::SwitchToMode(settings.DarkMode() ? DarkModeKind::Dark : DarkModeKind::Light, m_hWnd);
	UISetCheck(ID_OPTIONS_DARKMODE, settings.DarkMode());
	DrawMenuBar();
	SendMessageToDescendants(WM_UPDATE_DARKMODE);
	SendMessageToDescendants(::RegisterWindowMessage(L"WTLHelperUpdateTheme"));

	return 0;
}

LRESULT CMainFrame::OnAlwaysOnTop(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/) {
	auto alwaysOnTop = !(GetExStyle() & WS_EX_TOPMOST);
	SetAlwaysOnTop(alwaysOnTop);
	AppSettings::Get().AlwaysOnTop(alwaysOnTop);
	return 0;
}
