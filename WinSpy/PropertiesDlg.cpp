#include "pch.h"
#include "PropertiesDlg.h"
#include <ClipboardHelper.h>

LRESULT CPropertiesDlg::OnInitDialog(UINT, WPARAM, LPARAM, BOOL&) {
	SetWindowText(m_Title);
	DlgResize_Init(true);

	CListViewCtrl list(GetDlgItem(IDC_LIST));
	list.SetExtendedListViewStyle(LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER | LVS_EX_INFOTIP);
	// (the dialog may be on another monitor than the main window)
	auto dpi = ::GetDpiForWindow(m_hWnd);
	list.InsertColumn(0, L"Property", LVCFMT_LEFT, ::MulDiv(140, dpi, USER_DEFAULT_SCREEN_DPI));
	list.InsertColumn(1, L"Value", LVCFMT_LEFT, ::MulDiv(300, dpi, USER_DEFAULT_SCREEN_DPI));

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

CPropertyListPage::CPropertyListPage(PCWSTR title, UINT icon, HWND hWnd, Provider provider, bool live) :
	m_Title(title), m_Win(hWnd), m_Provider(std::move(provider)), m_Live(live) {
	m_psp.dwFlags |= PSP_USETITLE | PSP_USEICONID;
	m_psp.pszTitle = m_Title;
	m_psp.pszIcon = MAKEINTRESOURCE(icon);
}

LRESULT CPropertyListPage::OnInitDialog(UINT, WPARAM, LPARAM, BOOL&) {
	m_List.Attach(GetDlgItem(IDC_LIST));
	m_List.SetExtendedListViewStyle(LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER | LVS_EX_INFOTIP);
	CRect rc;
	m_List.GetClientRect(&rc);
	int width = rc.Width() / 3;
	m_List.InsertColumn(0, L"Property", LVCFMT_LEFT, width);
	m_List.InsertColumn(1, L"Value", LVCFMT_LEFT, rc.Width() - width - ::GetSystemMetrics(SM_CXVSCROLL));
	Refresh();
	// the list follows the page's size (the sheet is resizable; the layout is in the resources)
	InitDynamicLayout(false, false);
	return TRUE;
}

LRESULT CPropertyListPage::OnSize(UINT, WPARAM, LPARAM lParam, BOOL&) {
	UpdateDynamicLayout();
	// the values get the room
	if (m_List) {
		CRect rc;
		m_List.GetClientRect(&rc);
		m_List.SetColumnWidth(1, (std::max)(80, rc.Width() - m_List.GetColumnWidth(0)));
	}
	return 0;
}

int CPropertyListPage::OnSetActive() {
	Refresh();
	if (m_Live)
		SetTimer(1, 1000);
	return 0;
}

BOOL CPropertyListPage::OnKillActive() {
	KillTimer(1);
	return FALSE;
}

LRESULT CPropertyListPage::OnTimer(UINT, WPARAM id, LPARAM, BOOL&) {
	if (id == 1)
		Refresh();
	return 0;
}

LRESULT CPropertyListPage::OnWindowChanged(UINT, WPARAM, LPARAM lParam, BOOL&) {
	m_Win.Detach();
	m_Win.Attach(reinterpret_cast<HWND>(lParam));
	if (m_List)
		Refresh();
	return 0;
}

void CPropertyListPage::Refresh() {
	PropertyList properties;
	if (m_Win.IsWindow())
		properties = m_Provider(m_Win);
	else
		properties.push_back({ L"(The window has been destroyed)", L"" });
	if (properties == m_Properties)
		return;

	m_List.SetRedraw(FALSE);
	// the rows that are there are changed in place; the others added or removed
	int count = (int)properties.size();
	while (m_List.GetItemCount() > count)
		m_List.DeleteItem(m_List.GetItemCount() - 1);
	for (int i = 0; i < count; i++) {
		auto& [name, value] = properties[i];
		if (i >= m_List.GetItemCount())
			m_List.InsertItem(i, name);
		else if (i >= (int)m_Properties.size() || m_Properties[i].first != name)
			m_List.SetItemText(i, 0, name);
		if (i >= (int)m_Properties.size() || m_Properties[i].second != value)
			m_List.SetItemText(i, 1, value);
	}
	m_List.SetRedraw(TRUE);
	m_List.Invalidate();
	m_Properties = std::move(properties);
}
