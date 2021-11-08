#include "pch.h"
#include "resource.h"
#include "WindowsListView.h"
#include "FormatHelper.h"
#include "SortHelper.h"
#include "ProcessHelper.h"

void CWindowsListView::SetSelectedHwnd(HWND hWnd) {
	m_SelectedHwnd.Detach();
	m_SelectedHwnd.Attach(hWnd);
}

LRESULT CWindowsListView::OnCreate(UINT, WPARAM, LPARAM, BOOL&) {
	m_hWndClient = m_List.Create(m_hWnd, rcDefault, nullptr, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN
		| LVS_REPORT | LVS_OWNERDATA | LVS_SINGLESEL | LVS_SHAREIMAGELISTS, WS_EX_CLIENTEDGE);
	m_List.SetExtendedListViewStyle(LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER | LVS_EX_INFOTIP);
	m_List.SetImageList(WindowHelper::GetImageList(), LVSIL_SMALL);

	auto cm = GetColumnManager(m_List);

	struct {
		PCWSTR text;
		DataItemType type;
		int width = 100;
		int format = LVCFMT_LEFT;
		ColumnFlags flags = ColumnFlags::Visible;
	} columns[] = {
		{ L"Class Name", DataItemType::ClassName, 140 },
		{ L"Handle", DataItemType::Handle, 80, LVCFMT_RIGHT },
		{ L"Text", DataItemType::Text, 150 },
		{ L"Style", DataItemType::Style, 80, LVCFMT_RIGHT },
		{ L"Ex Style", DataItemType::ExtendedStyle, 80, LVCFMT_RIGHT },
		{ L"PID", DataItemType::ProcessId, 70, LVCFMT_RIGHT },
		{ L"Process Name", DataItemType::ProcessName, 120 },
		{ L"TID", DataItemType::ThreadId, 70, LVCFMT_RIGHT },
		{ L"Rectangle", DataItemType::Rectangle, 170, LVCFMT_LEFT, ColumnFlags::None },
		{ L"Parent HWND", DataItemType::ParentWindow, 100, LVCFMT_RIGHT, ColumnFlags::Fixed },
		{ L"Owner HWND", DataItemType::OwnerWindow, 100, LVCFMT_RIGHT, ColumnFlags::Fixed },
		{ L"Next HWND", DataItemType::NextWindow, 100, LVCFMT_RIGHT, ColumnFlags::Fixed },
		{ L"Previous HWND", DataItemType::PrevWindow, 100, LVCFMT_RIGHT, ColumnFlags::Fixed },
		{ L"First Child HWND", DataItemType::FirstChildWindow, 100, LVCFMT_RIGHT, ColumnFlags::Fixed },
		{ L"WndProc", DataItemType::WindowProc, 100, LVCFMT_RIGHT, ColumnFlags::Fixed },
		{ L"User Data", DataItemType::UserData, 100, LVCFMT_RIGHT, ColumnFlags::Fixed },
		{ L"ID", DataItemType::ID, 80, LVCFMT_RIGHT, ColumnFlags::Fixed },
		{ L"Class Atom", DataItemType::ClassAtom, 90, LVCFMT_RIGHT },
		{ L"Class Style", DataItemType::ClassStyle, 80, LVCFMT_RIGHT, ColumnFlags::Fixed },
		{ L"Class Extra Bytes", DataItemType::ClassExtra, 70, LVCFMT_RIGHT, ColumnFlags::Fixed },
		{ L"Window Extra Bytes", DataItemType::WindowExtra, 70, LVCFMT_RIGHT, ColumnFlags::Fixed },
	};

	for (auto& col : columns) {
		cm->AddColumn(col.text, col.format, col.width, col.type, col.flags);
	}

	cm->UpdateColumns();


    return 0;
}

LRESULT CWindowsListView::OnWindowShow(WORD, WORD, HWND, BOOL&) {
	CWindow win(m_Items[m_List.GetSelectionMark()].hWnd);
	win.ShowWindow(SW_SHOW);

	return 0;
}

LRESULT CWindowsListView::OnWindowHide(WORD, WORD, HWND, BOOL&) {
	ATLASSERT(m_SelectedHwnd);
	m_SelectedHwnd.ShowWindow(SW_HIDE);
	return 0;
}

LRESULT CWindowsListView::OnWindowMinimize(WORD, WORD, HWND, BOOL&) {
	CWindow win(m_Items[m_List.GetSelectionMark()].hWnd);
	win.ShowWindow(SW_MINIMIZE);
	return 0;
}

LRESULT CWindowsListView::OnWindowMaximize(WORD, WORD, HWND, BOOL&) {
	CWindow win(m_Items[m_List.GetSelectionMark()].hWnd);
	win.ShowWindow(SW_MAXIMIZE);

	return 0;
}

CString CWindowsListView::GetColumnText(HWND, int row, int col) const {
	auto& item = m_Items[row];
	if (!::IsWindow(item.hWnd)) {
		return L"";
	}

	CWindow win(item.hWnd);
	auto h = item.hWnd;

	CString text;
	switch (GetColumnManager(m_List)->GetColumnTag<DataItemType>(col)) {
		case DataItemType::Handle:
			text.Format(L"0x%zX", (ULONG_PTR)h);
			break;

		case DataItemType::Style:
			text.Format(L"0x%08X", win.GetStyle());
			break;

		case DataItemType::ProcessId:
			text.Format(L"%u", item.ProcessId);
			break;

		case DataItemType::ProcessName:
			return item.ProcessName;

		case DataItemType::ThreadId:
			text.Format(L"%u", item.ThreadId);
			break;

		case DataItemType::Rectangle:
			return WindowHelper::WindowRectToString(h);

		case DataItemType::ExtendedStyle:
			text.Format(L"0x%08X", win.GetExStyle());
			break;

		case DataItemType::ClassName:
			return WindowHelper::GetWindowClassName(h);

		case DataItemType::Text:
			win.GetWindowText(text);
			break;

		case DataItemType::WindowProc:
			text.Format(L"0x%zX", win.GetWindowLongPtr(GWLP_WNDPROC));
			break;

		case DataItemType::UserData:
			text.Format(L"0x%zX", win.GetWindowLongPtr(GWLP_USERDATA));
			break;

		case DataItemType::ID:
			text.Format(L"0x%zX", win.GetWindowLongPtr(GWLP_ID));
			break;

		case DataItemType::ParentWindow: return FormatHelper::FormatHWndOrNone(::GetAncestor(win, GA_PARENT));
		case DataItemType::NextWindow: return FormatHelper::FormatHWndOrNone(win.GetWindow(GW_HWNDNEXT));
		case DataItemType::PrevWindow: return FormatHelper::FormatHWndOrNone(win.GetWindow(GW_HWNDPREV));
		case DataItemType::OwnerWindow: return FormatHelper::FormatHWndOrNone(win.GetWindow(GW_OWNER));
		case DataItemType::FirstChildWindow: return FormatHelper::FormatHWndOrNone(win.GetWindow(GW_CHILD));

		case DataItemType::ClassAtom:
			text.Format(L"0x%04X", (DWORD)::GetClassLongPtr(win, GCW_ATOM));
			break;

		case DataItemType::ClassStyle:
			text.Format(L"0x%04X", (DWORD)::GetClassLongPtr(win, GCL_STYLE));
			break;

		case DataItemType::ClassExtra:
			text.Format(L"%u", (ULONG)::GetClassLongPtr(win, GCL_CBCLSEXTRA));
			break;

		case DataItemType::WindowExtra:
			text.Format(L"%u", (ULONG)::GetClassLongPtr(win, GCL_CBWNDEXTRA));
			break;
	}
	return text;
}

int CWindowsListView::GetRowImage(HWND, int row) const {
	auto& item = m_Items[row];
	auto h = item.hWnd;
	auto& icons = WindowHelper::GetIconMap();
	if (auto it = icons.find(h); it != icons.end()) {
		return it->second;
	}
	auto hIcon = WindowHelper::GetWindowIcon(h);
	if (hIcon) {
		int image = WindowHelper::GetImageList().AddIcon(hIcon);
		icons.insert({ h, image });
		return image;
	}
	return 0;
}

void CWindowsListView::DoSort(const SortInfo* si) {
	if (si == nullptr)
		return;

	std::sort(m_Items.begin(), m_Items.end(), [&](const auto& h1, const auto& h2) -> bool {
		switch (GetColumnManager(m_List)->GetColumnTag<DataItemType>(si->SortColumn)) {
			case DataItemType::ClassName: return SortHelper::SortStrings(WindowHelper::GetWindowClassName(h1.hWnd), WindowHelper::GetWindowClassName(h2.hWnd), si->SortAscending);
			case DataItemType::Text: return SortHelper::SortStrings(WindowHelper::GetWindowText(h1.hWnd), WindowHelper::GetWindowText(h2.hWnd), si->SortAscending);
			case DataItemType::Handle: return SortHelper::SortNumbers(h1.hWnd, h2.hWnd, si->SortAscending);
			case DataItemType::Style: return SortHelper::SortNumbers(CWindow(h1.hWnd).GetStyle(), CWindow(h2.hWnd).GetStyle(), si->SortAscending);
			case DataItemType::ExtendedStyle: return SortHelper::SortNumbers(CWindow(h1.hWnd).GetExStyle(), CWindow(h2.hWnd).GetExStyle(), si->SortAscending);
			case DataItemType::ThreadId: return SortHelper::SortNumbers(h1.ThreadId, h2.ThreadId, si->SortAscending);
			case DataItemType::ProcessId: return SortHelper::SortNumbers(h1.ProcessId, h2.ProcessId, si->SortAscending);
			case DataItemType::ProcessName: return SortHelper::SortStrings(h1.ProcessName, h2.ProcessName, si->SortAscending);
		}
		return false;
		});
}

bool CWindowsListView::OnRightClickList(HWND, int row, int col, CPoint const& pt) {
	CMenu menu;
	menu.LoadMenu(IDR_CONTEXT);
	m_ContextMenuOpen = true;
	auto cmd = m_pFrame->ShowContextMenu(menu.GetSubMenu(0), pt, TPM_RETURNCMD);
	m_ContextMenuOpen = false;
	if (cmd) {
		LRESULT result;
		return ProcessWindowMessage(m_hWnd, WM_COMMAND, cmd, 0, result, 1);
	}
	return false;
}

bool CWindowsListView::IsSortable(int col) const {
	return true;
}

DWORD CWindowsListView::OnPrePaint(int, LPNMCUSTOMDRAW cd) {
	if (cd->hdr.hwndFrom == m_List)
		return CDRF_NOTIFYITEMDRAW;

	return CDRF_DODEFAULT;
}

DWORD CWindowsListView::OnItemPrePaint(int, LPNMCUSTOMDRAW cd) {
	ATLASSERT(cd->hdr.hwndFrom == m_List);

	auto& h = m_Items[(int)cd->dwItemSpec];
	auto lv = (LPNMLVCUSTOMDRAW)cd;
	lv->clrTextBk = ::IsWindowVisible(h.hWnd) ? CLR_INVALID : RGB(224, 224, 224);

	return CDRF_DODEFAULT;
}

void CWindowsListView::UpdateList(HWND hWnd) {
	if (m_ContextMenuOpen)
		return;

	m_SelectedHwnd = hWnd;
	m_Items.clear();
	m_Items.push_back(WindowHelper::GetWindowInfo(m_SelectedHwnd));
	AddChildWindows(m_Items, m_SelectedHwnd, true);

	m_List.SetItemCountEx((int)m_Items.size(), LVSICF_NOSCROLL);
	UpdateList();
}

void CWindowsListView::UpdateListByThread(DWORD tid) {
	m_SelectedHwnd.Detach();
	m_Items.clear();
	AddThreadWindows(tid);

	m_List.SetItemCountEx((int)m_Items.size(), LVSICF_NOSCROLL);
	UpdateList();
}

void CWindowsListView::UpdateListByProcess(ProcessInfo const& pi) {
	m_Items.clear();
	for (auto tid : pi.Threads)
		AddThreadWindows(tid);

	m_List.SetItemCountEx((int)m_Items.size(), LVSICF_NOSCROLL);
	UpdateList();
}

void CWindowsListView::AddThreadWindows(DWORD tid) {
	::EnumThreadWindows(tid, [](auto hWnd, auto param) {
		auto p = reinterpret_cast<CWindowsListView*>(param);
		p->m_Items.push_back(WindowHelper::GetWindowInfo(hWnd));
		return TRUE;
		}, reinterpret_cast<LPARAM>(this));
}

void CWindowsListView::Refresh() {
}

void CWindowsListView::AddChildWindows(std::vector<WindowItem>& v, HWND hParent, bool directOnly) {
	struct LocalInfo {
		std::vector<WindowItem>& v;
		CWindowsListView* pThis;
		bool directOnly;
	};

	LocalInfo info{ v, this, directOnly };

	::EnumChildWindows(hParent, [](auto hWnd, auto param) {
		auto info = reinterpret_cast<LocalInfo*>(param);
		if (info->pThis->m_ShowHiddenWindows || ::IsWindowVisible(hWnd)) {
			info->v.push_back(WindowHelper::GetWindowInfo(hWnd));
			if (!info->directOnly)
				info->pThis->AddChildWindows(info->v, hWnd, false);
		}
		return TRUE;
		}, reinterpret_cast<LPARAM>(&info));
}

CString CWindowsListView::GetDetails(const DataItem& item) const {
	CString text;
	switch (item.Type) {
		case DataItemType::ProcessId:
		{
			DWORD pid = 0;
			::GetWindowThreadProcessId(m_SelectedHwnd, &pid);
			return ProcessHelper::GetProcessImageName(pid);
		}

		case DataItemType::Text:
			text.Format(L"Length: %d", m_SelectedHwnd.GetWindowTextLength());
			break;

			return WindowHelper::WindowStyleToString(m_SelectedHwnd);
		case DataItemType::Style: return WindowHelper::WindowStyleToString(m_SelectedHwnd);
		case DataItemType::ClassStyle: return WindowHelper::ClassStyleToString(m_SelectedHwnd);
		case DataItemType::ExtendedStyle: return WindowHelper::WindowExtendedStyleToString(m_SelectedHwnd);
		case DataItemType::Rectangle:
			if (m_SelectedHwnd.IsZoomed())
				return L"Maximized";
			if (m_SelectedHwnd.IsIconic())
				return L"Minimized";
			break;
		case DataItemType::NextWindow: return WindowHelper::GetWindowClassAndTitle(m_SelectedHwnd.GetWindow(GW_HWNDNEXT));
		case DataItemType::PrevWindow: return WindowHelper::GetWindowClassAndTitle(m_SelectedHwnd.GetWindow(GW_HWNDPREV));
		case DataItemType::OwnerWindow: return WindowHelper::GetWindowClassAndTitle(m_SelectedHwnd.GetWindow(GW_OWNER));
		case DataItemType::ParentWindow: return WindowHelper::GetWindowClassAndTitle(::GetAncestor(m_SelectedHwnd, GA_PARENT));
		case DataItemType::FirstChildWindow: return WindowHelper::GetWindowClassAndTitle(m_SelectedHwnd.GetWindow(GW_CHILD));
	}
	return text;
}

void CWindowsListView::UpdateList() {
	DoSort(GetSortInfo(m_List));
	m_List.RedrawItems(m_List.GetTopIndex(), m_List.GetCountPerPage() + m_List.GetTopIndex());
}

LRESULT CWindowsListView::OnToggleEmptyTitleWindows(WORD, WORD, HWND, BOOL&) {
	m_ShowNoTitleWindows = !m_ShowNoTitleWindows;
	UpdateList(m_SelectedHwnd);

	return 0;
}

LRESULT CWindowsListView::OnToggleChildWindows(WORD, WORD, HWND, BOOL&) {
	m_ShowChildWindows = !m_ShowChildWindows;
	UpdateList(m_SelectedHwnd);

	return 0;
}

LRESULT CWindowsListView::OnWindowRestore(WORD, WORD, HWND, BOOL&) {
	CWindow win(m_Items[m_List.GetSelectionMark()].hWnd);
	win.ShowWindow(SW_RESTORE);
	return 0;
}

LRESULT CWindowsListView::OnWindowBringToFront(WORD, WORD, HWND, BOOL&) {
	CWindow win(m_Items[m_List.GetSelectionMark()].hWnd);
	win.BringWindowToTop();
	return 0;
}

LRESULT CWindowsListView::OnWindowProperties(WORD, WORD, HWND, BOOL&) {
	auto& item = m_Items[m_List.GetSelectionMark()];
	WindowHelper::ShowWindowProperties(item.hWnd);
	return LRESULT();
}

LRESULT CWindowsListView::OnWindowFlash(WORD, WORD, HWND, BOOL&) {
	auto& item = m_Items[m_List.GetSelectionMark()];
	WindowHelper::Flash(item.hWnd);

	return 0;
}

LRESULT CWindowsListView::OnToggleHiddenWindows(WORD, WORD, HWND, BOOL&) {
	m_ShowHiddenWindows = !m_ShowHiddenWindows;
	UpdateList(m_SelectedHwnd);

	return 0;
}
