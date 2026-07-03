// DepWalk.cpp : main source file for DepWalk.exe
//

#include "pch.h"
#include "resource.h"
#include "MainFrm.h"
#include "View.h"
#include "AppSettings.h"
#include <WTLHelper.h>

CAppModule _Module;
AppSettings g_Settings;

int Run(LPCTSTR /*lpstrCmdLine*/ = NULL, int nCmdShow = SW_SHOWDEFAULT) {
	CMessageLoop theLoop;
	_Module.AddMessageLoop(&theLoop);

	CMainFrame wndMain;

	if (wndMain.CreateEx() == nullptr) {
		ATLTRACE(_T("Main window creation failed!\n"));
		return 0;
	}

	wndMain.ShowWindow(nCmdShow);

	int nRet = theLoop.Run();

	_Module.RemoveMessageLoop();
	return nRet;
}

int WINAPI _tWinMain(HINSTANCE hInstance, HINSTANCE /*hPrevInstance*/, LPTSTR lpstrCmdLine, int nCmdShow) {
	HRESULT hRes = ::CoInitialize(NULL);
	ATLASSERT(SUCCEEDED(hRes));

	AtlInitCommonControls(ICC_COOL_CLASSES | ICC_BAR_CLASSES | ICC_LISTVIEW_CLASSES);

	auto& settings = AppSettings::Get();
	settings.LoadFromKey(L"SOFTWARE\\ScorpioSoftware\\DepWalk");

	hRes = _Module.Init(NULL, hInstance);
	ATLASSERT(SUCCEEDED(hRes));

	WTLHelper::InitDarkMode(settings.DarkMode() ? DarkModeKind::Dark : DarkModeKind::Light);
	CView::SetAppFont(settings.Font());

	int nRet = Run(lpstrCmdLine, nCmdShow);

	_Module.Term();
	::CoUninitialize();

	return nRet;
}
