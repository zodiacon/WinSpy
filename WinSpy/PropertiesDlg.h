#pragma once

#include "resource.h"
#include "DialogHelper.h"
#include "FormatHelper.h"
#include <functional>

//
// a list of properties (name, value) - for processes and threads
//
class CPropertiesDlg :
	public CDialogImpl<CPropertiesDlg>,
	public CDialogResize<CPropertiesDlg>,
	public CDialogHelper<CPropertiesDlg> {
public:
	enum { IDD = IDD_PROPERTIES };

	using Properties = PropertyList;

	CPropertiesDlg(CString title, Properties properties) : m_Title(std::move(title)), m_Properties(std::move(properties)) {}

	BEGIN_DLGRESIZE_MAP(CPropertiesDlg)
		DLGRESIZE_CONTROL(IDC_LIST, DLSZ_SIZE_X | DLSZ_SIZE_Y)
		DLGRESIZE_CONTROL(IDC_COPY, DLSZ_MOVE_Y)
		DLGRESIZE_CONTROL(IDOK, DLSZ_MOVE_X | DLSZ_MOVE_Y)
	END_DLGRESIZE_MAP()

	BEGIN_MSG_MAP(CPropertiesDlg)
		MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
		COMMAND_ID_HANDLER(IDC_COPY, OnCopy)
		COMMAND_ID_HANDLER(IDOK, OnCloseCmd)
		COMMAND_ID_HANDLER(IDCANCEL, OnCloseCmd)
		CHAIN_MSG_MAP(CDialogResize<CPropertiesDlg>)
	END_MSG_MAP()

private:
	LRESULT OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnCopy(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnCloseCmd(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);

	CString m_Title;
	Properties m_Properties;
};

// sent by the Windows page to the other pages when it moves to another window (LPARAM: the window)
const UINT WM_PAGE_WINDOW_CHANGED = WM_USER + 300;

//
// a page of a window's properties (name, value), made by a function; refreshed while it's showing if 'live'
//
class CPropertyListPage :
	public CPropertyPageImpl<CPropertyListPage>,
	public CDynamicDialogLayout<CPropertyListPage> {
public:
	enum { IDD = IDD_PROPLIST };

	using Provider = std::function<PropertyList(HWND)>;

	CPropertyListPage(PCWSTR title, UINT icon, HWND hWnd, Provider provider, bool live = true);

	int OnSetActive();
	BOOL OnKillActive();

	BEGIN_MSG_MAP(CPropertyListPage)
		MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
		MESSAGE_HANDLER(WM_TIMER, OnTimer)
		MESSAGE_HANDLER(WM_SIZE, OnSize)
		MESSAGE_HANDLER(WM_PAGE_WINDOW_CHANGED, OnWindowChanged)
		CHAIN_MSG_MAP(CPropertyPageImpl<CPropertyListPage>)
		CHAIN_MSG_MAP(CDynamicDialogLayout<CPropertyListPage>)
	END_MSG_MAP()

private:
	// shows what the provider says now, changing only what changed (no flicker, the scroll position kept)
	void Refresh();

	LRESULT OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnTimer(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnSize(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnWindowChanged(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);

	CString m_Title;
	CWindow m_Win;
	Provider m_Provider;
	PropertyList m_Properties;
	CListViewCtrl m_List;
	bool m_Live;
};
