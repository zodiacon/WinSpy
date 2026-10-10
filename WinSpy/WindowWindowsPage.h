#pragma once

#include "DialogHelper.h"

class CWindowWindowsPage :
	public CDialogHelper<CWindowWindowsPage>,
	public CPropertyPageImpl<CWindowWindowsPage>,
	public CDynamicDialogLayout<CWindowWindowsPage> {
public:
	enum { IDD = IDD_PROPWINDOWS };

	CWindowWindowsPage(HWND hWnd) : m_Win(hWnd) {
		m_psp.dwFlags |= PSP_USEICONID;
		m_psp.pszIcon = MAKEINTRESOURCE(IDI_WINDOWS);
	}

	// refreshed every second while showing, if the related windows changed
	int OnSetActive();
	BOOL OnKillActive();

protected:
	BEGIN_MSG_MAP(CWindowWindowsPage)
		MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
		MESSAGE_HANDLER(WM_TIMER, OnTimer)
		NOTIFY_CODE_HANDLER(NM_CLICK, OnClickHandle)
		NOTIFY_CODE_HANDLER(NM_RETURN, OnClickHandle)
		CHAIN_MSG_MAP(CPropertyPageImpl<CWindowWindowsPage>)
		CHAIN_MSG_MAP(CDynamicDialogLayout<CWindowWindowsPage>)
	END_MSG_MAP()

private:
	void UpdateData();
	// the related windows (parent, next, previous, child), as text
	CString GetSnapshot() const;
	LRESULT OnTimer(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);

	//	LRESULT MessageHandler(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
	//	LRESULT CommandHandler(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	//	LRESULT NotifyHandler(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/)

	LRESULT OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnClickHandle(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/);

	CWindow m_Win;
	CString m_Snapshot;
};
