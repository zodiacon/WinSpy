#include "pch.h"
#include "PropertiesDlg.h"
#include <ClipboardHelper.h>

LRESULT CPropertiesDlg::OnInitDialog(UINT, WPARAM, LPARAM, BOOL&) {
	SetWindowText(m_Title);
	DlgResize_Init(true);

	CListViewCtrl list(GetDlgItem(IDC_LIST));
	list.SetExtendedListViewStyle(LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER | LVS_EX_INFOTIP);
	list.InsertColumn(0, L"Property", LVCFMT_LEFT, 140);
	list.InsertColumn(1, L"Value", LVCFMT_LEFT, 300);

	int i = 0;
	for (auto& [name, value] : m_Properties) {
		list.InsertItem(i, name);
		list.SetItemText(i, 1, value);
		i++;
	}
	list.SetColumnWidth(1, LVSCW_AUTOSIZE_USEHEADER);

	return TRUE;
}

LRESULT CPropertiesDlg::OnCopy(WORD, WORD, HWND, BOOL&) {
	CString text;
	for (auto& [name, value] : m_Properties)
		text += name + L": " + value + L"\r\n";
	ClipboardHelper::CopyText(m_hWnd, text);
	return 0;
}

LRESULT CPropertiesDlg::OnCloseCmd(WORD, WORD wID, HWND, BOOL&) {
	EndDialog(wID);
	return 0;
}
