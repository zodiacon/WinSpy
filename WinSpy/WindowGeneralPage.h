#pragma once

#include "DialogHelper.h"

struct StyleItem;

class CWindowGeneralPage :
	public CDialogHelper<CWindowGeneralPage>,
	public CPropertyPageImpl<CWindowGeneralPage> {
public:
	enum { IDD = IDD_WINPROP };

	CWindowGeneralPage(HWND hWnd) : m_Win(hWnd) {}

protected:
	BEGIN_MSG_MAP(CWindowGeneralPage)
		MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
	END_MSG_MAP()

private:
	void FillStyleList(DWORD style, std::pair<StyleItem const*, int> styles, UINT id, PCWSTR prefix, PCWSTR defaultStyle);

//	LRESULT MessageHandler(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
//	LRESULT CommandHandler(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
//	LRESULT NotifyHandler(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/)

	LRESULT OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);

	CWindow m_Win;
};
