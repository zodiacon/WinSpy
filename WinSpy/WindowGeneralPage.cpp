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

void CWindowGeneralPage::FillStyleList(DWORD style, std::pair<StyleItem const*, int> styles, UINT id, PCWSTR prefix, PCWSTR defaultStyle) {
	auto const [items, count] = styles;
	CListBox lb(GetDlgItem(id));
	ATLASSERT(lb);

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
	WCHAR name[64];
	if (!::GetClassName(m_Win, name, _countof(name)))
		return;

	auto style = m_Win.GetStyle() & 0xffff;
	const CString ATLPrefix = L"ATL:";

	static const struct {
		PCWSTR className;
		std::pair<StyleItem const*, int> items;
		PCWSTR def = L"";
	} controls[] = {
		{ WC_LISTVIEW, WindowHelper::GetListViewStyleArray(), L"LVS_ALIGNTOP" },
		{ WC_TREEVIEW, WindowHelper::GetTreeViewStyleArray() },
		{ WC_TABCONTROL, WindowHelper::GetTabCtrlStyleArray(), L"TCS_TABS" },
		{ WC_LISTBOX, WindowHelper::GetListBoxStyleArray(), L"" },
		{ WC_COMBOBOX, WindowHelper::GetComboBoxStyleArray(), L"" },
		{ WC_EDIT, WindowHelper::GetEditStyleArray(), L"ES_LEFT" },
		{ WC_BUTTON, WindowHelper::GetButtonStyleArray(), L"BS_TEXT" },
		{ WC_HEADER, WindowHelper::GetHeaderStyleArray(), L"HDS_HORZ" },
		{ WC_STATIC, WindowHelper::GetStaticStyleArray(), L"SS_LEFT" },
		{ TOOLTIPS_CLASS, WindowHelper::GetToolTipStyleArray(), L"" },
		{ STATUSCLASSNAME, WindowHelper::GetStatusBarStyleArray(), L"" },
		{ TOOLBARCLASSNAME, WindowHelper::GetToolBarStyleArray(), L"" },
		{ REBARCLASSNAME, WindowHelper::GetRebarStyleArray(), L"" },
	};

	for (auto& item : controls) {
		if (_wcsicmp(name, item.className) == 0 || _wcsicmp(name, ATLPrefix + item.className) == 0) {
			FillStyleList(style, item.items, IDC_STYLES, L"", item.def);
			break;
		}
	}
}

LRESULT CWindowGeneralPage::OnInitDialog(UINT, WPARAM, LPARAM, BOOL&) {
	UpdateData();

	return 0;
}

LRESULT CWindowGeneralPage::OnUpdate(UINT, WPARAM, LPARAM lp, BOOL&) {
	ATLASSERT(lp);
	m_Win.Detach();
	m_Win.Attach(reinterpret_cast<HWND>(lp));
	UpdateData();

	return 0;
}
