#include "pch.h"
#include "resource.h"
#include "WindowsListView.h"
#include "FormatHelper.h"
#include "SortHelper.h"
#include "ProcessHelper.h"
#include <WTLHelper.h>

void CWindowsListView::SetSelectedHwnd(HWND hWnd) {
	m_SelectedHwnd.Detach();
	m_SelectedHwnd.Attach(hWnd);
}

LRESULT CWindowsListView::OnCreate(UINT, WPARAM, LPARAM, BOOL&) {
	m_hWndClient = m_List.Create(m_hWnd, rcDefault, nullptr, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN
		| LVS_REPORT | LVS_OWNERDATA | LVS_SINGLESEL | LVS_SHAREIMAGELISTS, 0);
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
		{ L"Parent HWND", DataItemType::ParentWindow, 100, LVCFMT_RIGHT, ColumnFlags::Fixed | ColumnFlags::Visible },
		{ L"Owner HWND", DataItemType::OwnerWindow, 100, LVCFMT_RIGHT, ColumnFlags::Fixed | ColumnFlags::Visible },
		{ L"Next HWND", DataItemType::NextWindow, 100, LVCFMT_RIGHT, ColumnFlags::Fixed | ColumnFlags::Visible },
		{ L"Prev HWND", DataItemType::PrevWindow, 100, LVCFMT_RIGHT, ColumnFlags::Fixed | ColumnFlags::Visible },
		{ L"Child HWND", DataItemType::FirstChildWindow, 100, LVCFMT_RIGHT, ColumnFlags::Fixed | ColumnFlags::Visible },
		{ L"WndProc", DataItemType::WindowProc, 100, LVCFMT_RIGHT, ColumnFlags::Fixed },
		{ L"User Data", DataItemType::UserData, 100, LVCFMT_RIGHT },
		{ L"ID", DataItemType::ID, 80, LVCFMT_RIGHT },
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

HWND CWindowsListView::GetSelectedListHwnd() const {
	int index = m_List.GetNextItem(-1, LVNI_SELECTED);
	return index >= 0 && index < (int)m_Items.size() ? m_Items[index].hWnd : nullptr;
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
			if (text.GetLength() > 256)
				text = text.Left(256) + L"...";
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

int CWindowsListView::GetRowImage(HWND, int row, int col) const {
	auto& item = m_Items[row];
	auto h = item.hWnd;
	auto& icons = WindowHelper::GetIconMap();
	if (auto it = icons.find(h); it != icons.end()) {
		return it->second;
	}
	//
	// windows without an icon are remembered too, so they're not asked again (across processes) on every repaint
	//
	if (m_NoIcon.contains(h))
		return 0;

	auto hIcon = WindowHelper::GetWindowIcon(h);
	if (hIcon) {
		int image = WindowHelper::GetImageList().AddIcon(hIcon);
		icons.insert({ h, image });
		return image;
	}
	m_NoIcon.insert(h);
	return 0;
}

void CWindowsListView::DoSort(const SortInfo* si) {
	if (si == nullptr)
		return;

	//
	// the keys are read once before sorting: a window can change (or be destroyed) during the sort,
	// and the comparison must stay consistent
	//
	struct Key {
		CString Text;
		ULONG_PTR Value{ 0 };
	};
	auto type = GetColumnManager(m_List)->GetColumnTag<DataItemType>(si->SortColumn);
	auto getKey = [type](WindowItem const& item) -> Key {
		auto h = item.hWnd;
		switch (type) {
			case DataItemType::ClassName: return { WindowHelper::GetWindowClassName(h) };
			case DataItemType::Text: return { WindowHelper::GetWindowText(h) };
			case DataItemType::ProcessName: return { item.ProcessName };
			case DataItemType::Handle: return { {}, (ULONG_PTR)h };
			case DataItemType::FirstChildWindow: return { {}, (ULONG_PTR)::GetWindow(h, GW_CHILD) };
			case DataItemType::PrevWindow: return { {}, (ULONG_PTR)::GetWindow(h, GW_HWNDPREV) };
			case DataItemType::NextWindow: return { {}, (ULONG_PTR)::GetWindow(h, GW_HWNDNEXT) };
			case DataItemType::OwnerWindow: return { {}, (ULONG_PTR)::GetWindow(h, GW_OWNER) };
			case DataItemType::ParentWindow: return { {}, (ULONG_PTR)::GetAncestor(h, GA_PARENT) };
			case DataItemType::Style: return { {}, (ULONG_PTR)CWindow(h).GetStyle() };
			case DataItemType::ExtendedStyle: return { {}, (ULONG_PTR)CWindow(h).GetExStyle() };
			case DataItemType::ThreadId: return { {}, item.ThreadId };
			case DataItemType::ProcessId: return { {}, item.ProcessId };
			case DataItemType::ID: return { {}, WindowHelper::GetID(h) };
			case DataItemType::UserData: return { {}, WindowHelper::GetUserData(h) };
		}
		return {};
	};
	bool text = type == DataItemType::ClassName || type == DataItemType::Text || type == DataItemType::ProcessName;

	std::vector<std::pair<Key, WindowItem>> items;
	items.reserve(m_Items.size());
	for (auto& item : m_Items)
		items.push_back({ getKey(item), std::move(item) });

	std::sort(items.begin(), items.end(), [&](const auto& i1, const auto& i2) {
		return text ? SortHelper::Sort(i1.first.Text, i2.first.Text, si->SortAscending)
			: SortHelper::Sort(i1.first.Value, i2.first.Value, si->SortAscending);
		});

	m_Items.clear();
	for (auto& item : items)
		m_Items.push_back(std::move(item.second));
}

bool CWindowsListView::OnRightClickList(HWND, int row, int col, CPoint const& pt) {
	CMenu menu;
	menu.LoadMenu(IDR_CONTEXT);
	m_ContextMenuOpen = true;
	// the chosen command goes to the frame (WM_COMMAND), which passes it to the view
	m_pFrame->ShowPopupMenu(menu.GetSubMenu(0), pt);
	m_ContextMenuOpen = false;
	return true;
}

bool CWindowsListView::IsSortable(HWND, int col) const {
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
	// hidden windows are shaded
	lv->clrTextBk = m_SelectedHwnd == nullptr || ::IsWindowVisible(h.hWnd) ? CLR_INVALID
		: WTLHelper::IsDarkMode() ? RGB(64, 64, 64) : RGB(224, 224, 224);

	return CDRF_DODEFAULT;
}

void CWindowsListView::AddMessageOnlyWindows() {
	HWND hWnd = nullptr;
	for (;;) {
		hWnd = ::FindWindowEx(HWND_MESSAGE, hWnd, nullptr, nullptr);
		if (!hWnd)
			break;
		
		m_Items.push_back(WindowHelper::GetWindowInfo(hWnd));
	}
}

void CWindowsListView::UpdateList(HWND hWnd) {
	if (m_ContextMenuOpen)
		return;

	m_Mode = ListMode::Window;
	m_Items.clear();
	m_SelectedHwnd = hWnd;
	std::erase_if(m_NoIcon, [](auto h) { return !::IsWindow(h); });
	if (hWnd == nullptr) {
		//
		// message only windows
		//
		AddMessageOnlyWindows();
	}
	else {
		m_Items.push_back(WindowHelper::GetWindowInfo(m_SelectedHwnd));
		AddChildWindows(m_Items, m_SelectedHwnd, true);
	}
	m_List.SetItemCountEx((int)m_Items.size(), LVSICF_NOSCROLL);
	UpdateList();
}

void CWindowsListView::UpdateListByThread(DWORD tid) {
	m_Mode = ListMode::Thread;
	m_ModeThread = tid;
	m_SelectedHwnd.Detach();
	m_Items.clear();
	AddThreadWindows(tid);

	m_List.SetItemCountEx((int)m_Items.size(), LVSICF_NOSCROLL);
	UpdateList();
}

void CWindowsListView::UpdateListByProcess(ProcessInfo const& pi) {
	m_Mode = ListMode::Process;
	m_ModeProcess = pi;
	m_SelectedHwnd.Detach();
	m_Items.clear();
	for (auto tid : pi.Threads)
		AddThreadWindows(tid);

	m_List.SetItemCountEx((int)m_Items.size(), LVSICF_NOSCROLL);
	UpdateList();
}

void CWindowsListView::UpdateUI(CUpdateUIBase& ui) {
	if (::GetFocus() != m_List)
		return;

	int first = m_List.GetSelectionMark();
	int selected = m_List.GetSelectedCount();
	ui.UIEnable(ID_WINDOW_PROPERTIES, selected == 1);
}

void CWindowsListView::AddThreadWindows(DWORD tid) {
	::EnumThreadWindows(tid, [](auto hWnd, auto param) {
		auto p = reinterpret_cast<CWindowsListView*>(param);
		p->m_Items.push_back(WindowHelper::GetWindowInfo(hWnd));
		return TRUE;
		}, reinterpret_cast<LPARAM>(this));
}

void CWindowsListView::UpdateListAll() {
	m_Mode = ListMode::All;
	m_SelectedHwnd.Detach();
	m_Items.clear();
	::EnumWindows([](auto hWnd, auto param) {
		auto p = reinterpret_cast<CWindowsListView*>(param);
		if (!p->m_ShowHiddenWindows && !::IsWindowVisible(hWnd))
			return TRUE;
		if (!p->m_ShowNoTitleWindows && ::GetWindowTextLength(hWnd) == 0)
			return TRUE;
		p->m_Items.push_back(WindowHelper::GetWindowInfo(hWnd));
		return TRUE;
		}, reinterpret_cast<LPARAM>(this));

	m_List.SetItemCountEx((int)m_Items.size(), LVSICF_NOSCROLL);
	UpdateList();
}

void CWindowsListView::Refresh() {
	if (m_ContextMenuOpen)
		return;

	switch (m_Mode) {
		case ListMode::Window: UpdateList(m_SelectedHwnd); break;
		case ListMode::Thread: UpdateListByThread(m_ModeThread); break;
		case ListMode::Process: UpdateListByProcess(m_ModeProcess); break;
		case ListMode::All: UpdateListAll(); break;
	}
}

void CWindowsListView::AddChildWindows(std::vector<WindowItem>& v, HWND hParent, bool directOnly) {
	struct LocalInfo {
		std::vector<WindowItem>& v;
		CWindowsListView* pThis;
		HWND hParent;
		bool directOnly;
	};

	LocalInfo info{ v, this, hParent, directOnly };

	//
	// EnumChildWindows goes through all the descendants (so no recursion is needed)
	//
	::EnumChildWindows(hParent, [](auto hWnd, auto param) {
		auto info = reinterpret_cast<LocalInfo*>(param);
		if (info->directOnly && ::GetAncestor(hWnd, GA_PARENT) != info->hParent)
			return TRUE;
		if (info->pThis->m_ShowHiddenWindows || ::IsWindowVisible(hWnd))
			info->v.push_back(WindowHelper::GetWindowInfo(hWnd));
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
	Refresh();

	return 0;
}

LRESULT CWindowsListView::OnToggleChildWindows(WORD, WORD, HWND, BOOL&) {
	m_ShowChildWindows = !m_ShowChildWindows;
	Refresh();

	return 0;
}

LRESULT CWindowsListView::OnItemChanged(int, LPNMHDR, BOOL&) {
	UpdateUI(m_pFrame->GetUIUpdate());
	return 0;
}

LRESULT CWindowsListView::OnToggleHiddenWindows(WORD, WORD, HWND, BOOL&) {
	m_ShowHiddenWindows = !m_ShowHiddenWindows;
	Refresh();

	return 0;
}

bool CWindowsListView::OnDoubleClickList(HWND, int row, int col, CPoint const&) {
	if (row >= 0) {
		WindowHelper::ShowWindowProperties(m_Items[row].hWnd);
		return true;
	}
	return false;
}

