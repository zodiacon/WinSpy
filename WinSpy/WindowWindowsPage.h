#pragma once

#include "DialogHelper.h"

class CWindowWindowsPage :
	public CDialogHelper<CWindowWindowsPage>,
	public CPropertyPageImpl<CWindowWindowsPage> {
public:
	enum { IDD = IDD_PROPWINDOWS };

	CWindowWindowsPage(HWND hWnd) : m_Win(hWnd) {
		m_psp.dwFlags |= PSP_USEICONID;
		m_psp.pszIcon = MAKEINTRESOURCE(IDI_WINDOWS);
	}

protected:
	BEGIN_MSG_MAP(CWindowWindowsPage)
		MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
		NOTIFY_CODE_HANDLER(NM_CLICK, OnClickHandle)
		NOTIFY_CODE_HANDLER(NM_RETURN, OnClickHandle)
	END_MSG_MAP()

private:
	void UpdateData();

	//	LRESULT MessageHandler(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
	//	LRESULT CommandHandler(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	//	LRESULT NotifyHandler(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/)

	LRESULT OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnClickHandle(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/);

	CWindow m_Win;
};
