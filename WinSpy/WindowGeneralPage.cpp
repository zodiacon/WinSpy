#include "pch.h"
#include "resource.h"
#include "WindowGeneralPage.h"
#include "FormatHelper.h"
#include "ProcessHelper.h"
#include "WindowHelper.h"

void CWindowGeneralPage::UpdateData() {
	WINDOWINFO wi{ sizeof(wi) };
	::GetWindowInfo(m_Win, &wi);
	GetParent().SetIcon(WindowHelper::GetWindowIcon(m_Win));

	SetDlgItemText(IDC_RECT, FormatHelper::RectToString(wi.rcWindow));
	SetDlgItemText(IDC_CLIENTRECT, FormatHelper::RectToString(wi.rcClient));
	DWORD pid;
	auto tid = ::GetWindowThreadProcessId(m_Win, &pid);
	SetDlgItemInt(IDC_THREAD, tid, FALSE);
	CString text;
	text.Format(L"0x%zX", DWORD_PTR(m_Win.m_hWnd));
	SetDlgItemText(IDC_HANDLE, text);

	if (tid) {
		text.Format(L"%s (%u)", (PCWSTR)ProcessHelper::GetProcessImageName(pid), pid);
		SetDlgItemText(IDC_PROCESS, text);
	}
	WINDOWPLACEMENT wp{ sizeof(wp) };
	m_Win.GetWindowPlacement(&wp);
	SetDlgItemText(IDC_RESTORE_RECT, FormatHelper::RectToString(wp.rcNormalPosition));

	m_Win.GetWindowText(text);
	SetDlgItemText(IDC_TEXT, text);
	text.Format(L"0x%04X\n", wi.atomWindowType);
	SetDlgItemText(IDC_ATOM, text);
	text.Format(L"0x%08X\n", wi.dwStyle);
	SetDlgItemText(IDC_STYLE, text);
	text.Format(L"0x%08X\n", wi.dwExStyle);
	SetDlgItemText(IDC_STYLEEX, text);
	text.Format(L"0x%zX", m_Win.GetMenu() ? (ULONG_PTR)m_Win.GetMenu() : WindowHelper::GetID(m_Win));
	SetDlgItemText(IDC_MENU, text);
	text.Format(L"0x%zX", m_Win.GetWindowLongPtr(GWLP_USERDATA));
	SetDlgItemText(IDC_USERDATA, text);

	WCHAR className[128];
	if (::GetClassName(m_Win, className, _countof(className)))
		SetDlgItemText(IDC_CLASSNAME, className);

	auto clsStyle = (DWORD)::GetClassLongPtr(m_Win, GCL_STYLE);
	text.Format(L"0x%08X", clsStyle);
	SetDlgItemText(IDC_STYLECLASS, text);
	FillStyleList(wi.dwStyle, WindowHelper::GetWindowStyleArray(), IDC_STYLES, L"", L"WS_OVERLAPPED");
	FillSpecificControlStyles();
	FillStyleList(wi.dwExStyle, WindowHelper::GetWindowStyleExArray(), IDC_STYLESEX, L"", L"WS_EX_LEFT");
	FillStyleList(clsStyle, WindowHelper::GetClassStyleArray(), IDC_CLASSSTYLE, L"", nullptr);
}

void CWindowGeneralPage::FillStyleList(DWORD style, std::pair<StyleItem const*, int> styles, UINT id, PCWSTR prefix, PCWSTR defaultStyle, bool clear) {
	auto const [items, count] = styles;
	CListBox lb(GetDlgItem(id));
	ATLASSERT(lb);
	if(clear)
		lb.ResetContent();

	CString text;
	for (int i = 0; i < count; i++) {
		if ((style & items[i].Mask) == items[i].Value) {
			text.Format(L"%s%s (0x%X)", prefix, (PCWSTR)items[i].Text, items[i].Value);
			lb.AddString(text);
		}
	}
	if (style == 0 && defaultStyle)
		lb.AddString(CString(defaultStyle) + L" (0)");
}

void CWindowGeneralPage::FillSpecificControlStyles() {
	PCWSTR defaultStyle;
	auto styles = WindowHelper::GetControlStyleArray(m_Win, &defaultStyle);
	if (styles.second)
		FillStyleList(m_Win.GetStyle() & 0xffff, styles, IDC_STYLES, L"", defaultStyle, false);
}

LRESULT CWindowGeneralPage::OnInitDialog(UINT, WPARAM, LPARAM, BOOL&) {
	UpdateData();
	m_Snapshot = GetSnapshot();
	// the controls follow the page's size (the sheet is resizable; the layout is in the resources)
	InitDynamicLayout(false, false);

	return 0;
}

CString CWindowGeneralPage::GetSnapshot() const {
	WINDOWINFO wi{ sizeof(wi) };
	::GetWindowInfo(m_Win, &wi);
	WINDOWPLACEMENT wp{ sizeof(wp) };
	::GetWindowPlacement(m_Win, &wp);
	CString text, snapshot;
	CWindow(m_Win).GetWindowText(text);
	snapshot.Format(L"%s|%s|%s|%s|%X|%X|%X|%zX", (PCWSTR)text, (PCWSTR)FormatHelper::RectToString(wi.rcWindow),
		(PCWSTR)FormatHelper::RectToString(wi.rcClient), (PCWSTR)FormatHelper::RectToString(wp.rcNormalPosition),
		wi.dwStyle, wi.dwExStyle, (DWORD)::GetClassLongPtr(m_Win, GCL_STYLE), ::GetWindowLongPtr(m_Win, GWLP_USERDATA));
	return snapshot;
}

int CWindowGeneralPage::OnSetActive() {
	SetTimer(1, 1000);
	return 0;
}

BOOL CWindowGeneralPage::OnKillActive() {
	KillTimer(1);
	return FALSE;
}

LRESULT CWindowGeneralPage::OnTimer(UINT, WPARAM id, LPARAM, BOOL&) {
	if (id != 1 || !m_Win.IsWindow())
		return 0;
	// only when something changed: filling the style lists again would lose their scroll position
	if (auto snapshot = GetSnapshot(); snapshot != m_Snapshot) {
		m_Snapshot = snapshot;
		UpdateData();
	}
	return 0;
}

LRESULT CWindowGeneralPage::OnUpdate(UINT, WPARAM, LPARAM lp, BOOL&) {
	ATLASSERT(lp);
	m_Win.Detach();
	m_Win.Attach(reinterpret_cast<HWND>(lp));
	UpdateData();
	m_Snapshot = GetSnapshot();

	return 0;
}

// ---------------- styles

LRESULT CWindowStylesPage::OnInitDialog(UINT, WPARAM, LPARAM, BOOL&) {
	m_Styles.List.Attach(GetDlgItem(IDC_STYLES));
	m_Styles.EditId = IDC_STYLE;
	m_ExStyles.List.Attach(GetDlgItem(IDC_STYLESEX));
	m_ExStyles.EditId = IDC_STYLEEX;
	for (auto list : { &m_Styles.List, &m_ExStyles.List }) {
		list->SetExtendedListViewStyle(LVS_EX_CHECKBOXES | LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER);
		CRect rc;
		list->GetClientRect(&rc);
		list->InsertColumn(0, L"", LVCFMT_LEFT, rc.Width() - ::GetSystemMetrics(SM_CXVSCROLL));
	}
	Load();
	InitDynamicLayout(false, false);
	return TRUE;
}

LRESULT CWindowStylesPage::OnSize(UINT, WPARAM, LPARAM lParam, BOOL&) {
	UpdateDynamicLayout();
	// the one column as wide as its list
	for (auto list : { &m_Styles.List, &m_ExStyles.List }) {
		if (*list) {
			CRect rc;
			list->GetClientRect(&rc);
			list->SetColumnWidth(0, rc.Width());
		}
	}
	return 0;
}

void CWindowStylesPage::Load() {
	m_Styles.Original = m_Styles.Value = m_Win.GetStyle();
	m_ExStyles.Original = m_ExStyles.Value = m_Win.GetExStyle();

	std::vector<std::pair<StyleItem const*, int>> styles{ WindowHelper::GetWindowStyleArray() };
	if (auto control = WindowHelper::GetControlStyleArray(m_Win); control.second)
		styles.push_back(control);
	FillList(m_Styles, styles);
	FillList(m_ExStyles, { WindowHelper::GetWindowStyleExArray() });
	GetDlgItem(IDC_APPLY).EnableWindow(FALSE);
	GetDlgItem(IDC_RESET).EnableWindow(FALSE);
}

void CWindowStylesPage::FillList(StyleList& styles, std::vector<std::pair<StyleItem const*, int>> const& arrays) {
	m_Updating = true;
	styles.Items.clear();
	styles.List.DeleteAllItems();
	CString text;
	for (auto [items, count] : arrays) {
		for (int i = 0; i < count; i++) {
			// a style of value 0 is what's left when others are cleared: nothing to check
			if (items[i].Value == 0)
				continue;
			styles.Items.push_back(items[i]);
			text.Format(L"%s (0x%X)", items[i].Text, items[i].Value);
			styles.List.InsertItem(styles.List.GetItemCount(), text);
		}
	}
	m_Updating = false;
	ShowStyle(styles);
}

void CWindowStylesPage::ShowStyle(StyleList& styles) {
	m_Updating = true;
	for (int i = 0; i < (int)styles.Items.size(); i++) {
		auto& item = styles.Items[i];
		styles.List.SetCheckState(i, (styles.Value & item.Mask) == item.Value);
	}
	m_Updating = false;

	CString text;
	text.Format(L"0x%08X", styles.Value);
	if (styles.Value != styles.Original)
		text += L" *";
	SetDlgItemText(styles.EditId, text);
}

bool CWindowStylesPage::IsChanged() const {
	return m_Styles.Value != m_Styles.Original || m_ExStyles.Value != m_ExStyles.Original;
}

LRESULT CWindowStylesPage::OnItemChanged(int id, LPNMHDR hdr, BOOL&) {
	auto lv = reinterpret_cast<NMLISTVIEW*>(hdr);
	// only the user's changes to a check box
	if (m_Updating || (lv->uChanged & LVIF_STATE) == 0 || ((lv->uNewState ^ lv->uOldState) & LVIS_STATEIMAGEMASK) == 0)
		return 0;

	auto& styles = id == IDC_STYLES ? m_Styles : m_ExStyles;
	if (lv->iItem < 0 || lv->iItem >= (int)styles.Items.size())
		return 0;
	auto& item = styles.Items[lv->iItem];
	if (styles.List.GetCheckState(lv->iItem))
		styles.Value = (styles.Value & ~item.Mask) | item.Value;
	else
		styles.Value &= ~item.Value;

	// other check boxes may share bits with this one
	ShowStyle(styles);
	GetDlgItem(IDC_APPLY).EnableWindow(IsChanged());
	GetDlgItem(IDC_RESET).EnableWindow(IsChanged());
	return 0;
}

LRESULT CWindowStylesPage::OnApplyStyles(WORD, WORD, HWND, BOOL&) {
	DWORD error = 0;
	for (auto [styles, index] : { std::pair{ &m_Styles, GWL_STYLE }, std::pair{ &m_ExStyles, GWL_EXSTYLE } }) {
		if (styles->Value == styles->Original)
			continue;
		::SetLastError(0);
		if (m_Win.SetWindowLongPtr(index, styles->Value) == 0 && ::GetLastError())
			error = ::GetLastError();
	}
	// the frame is recalculated and the window redrawn, which some styles need
	m_Win.SetWindowPos(nullptr, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
	m_Win.RedrawWindow(nullptr, nullptr, RDW_INVALIDATE | RDW_ERASE | RDW_FRAME | RDW_ALLCHILDREN);
	if (error) {
		CString text;
		text.Format(L"Failed to change the styles (error %u)", error);
		AtlMessageBox(m_hWnd, (PCWSTR)text, IDS_TITLE, MB_ICONERROR);
	}
	// what the window has now (it may not take every change)
	Load();
	return 0;
}

LRESULT CWindowStylesPage::OnRevertStyles(WORD, WORD, HWND, BOOL&) {
	Load();
	return 0;
}

int CWindowStylesPage::OnSetActive() {
	SetTimer(1, 1000);
	return 0;
}

BOOL CWindowStylesPage::OnKillActive() {
	KillTimer(1);
	return FALSE;
}

LRESULT CWindowStylesPage::OnTimer(UINT, WPARAM id, LPARAM, BOOL&) {
	// the window's styles changed, and there are no changes of the user's to lose
	if (id == 1 && m_Win.IsWindow() && !IsChanged() && (m_Win.GetStyle() != m_Styles.Original || m_Win.GetExStyle() != m_ExStyles.Original))
		Load();
	return 0;
}

LRESULT CWindowStylesPage::OnUpdate(UINT, WPARAM, LPARAM lp, BOOL&) {
	m_Win.Detach();
	m_Win.Attach(reinterpret_cast<HWND>(lp));
	if (m_Styles.List)
		Load();
	return 0;
}
