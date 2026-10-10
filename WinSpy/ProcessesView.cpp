#include "pch.h"
#include "resource.h"
#include "ProcessesView.h"
#include "ImageIconCache.h"

LRESULT CProcessesView::OnCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/) {
	m_Splitter.SetSplitterExtendedStyle(SPLIT_FLATBAR | SPLIT_PROPORTIONAL);
	m_hWndClient = m_Splitter.Create(m_hWnd, rcDefault, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN);

	m_Tree.Create(m_Splitter, rcDefault, nullptr, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN |
		TVS_HASLINES | TVS_LINESATROOT | TVS_HASBUTTONS | TVS_SHOWSELALWAYS, 0, IDC_TREE);
	m_WindowsView.Create(m_Splitter, rcDefault, nullptr, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN);
	m_Tree.SetExtendedStyle(TVS_EX_DOUBLEBUFFER, TVS_EX_DOUBLEBUFFER);

	m_Tree.SetImageList(WindowHelper::GetImageList(), TVSIL_NORMAL);

	m_Splitter.SetSplitterPanes(m_Tree, m_WindowsView);
	UpdateLayout();
	m_Splitter.SetSplitterPosPct(35);

	InitTree();

	return 0;
}

void CProcessesView::OnActivate(bool active) {
	if (active) {
		UpdateUI();
	}
}


LRESULT CProcessesView::OnTimer(UINT /*uMsg*/, WPARAM id, LPARAM /*lParam*/, BOOL& /*bHandled*/) {
	if (id == 3) {
		KillTimer(3);
		ChangeSelection(m_Tree.GetSelectedItem());
	}
	return 0;
}

void CProcessesView::InitTree() {
	WindowHelper::PruneIconMap();
	m_Tree.LockWindowUpdate();
	m_Tree.DeleteAllItems();
	m_Processes.clear();
	m_WindowMap.clear();
	m_Threads.clear();
	m_Selected = CTreeItem();
	m_SelectedHwnd = nullptr;
	m_hCurrentNode = CTreeItem();
	m_TotalWindows = m_TotalVisibleWindows = m_TopLevelWindows = 0;

	CString text;
	auto info = ProcessHelper::EnumProcessesAndThreads(
		EnumProcessesOptions::UIThreadsOnly | EnumProcessesOptions::SkipProcessesWithNoUI | EnumProcessesOptions::IncludeMessageOnly);
	for (auto& pi : info.Processes) {
		auto icon = ImageIconCache::Get().GetIcon(pi.FullPath);
		text.Format(L"%s (%u)", (PCWSTR)pi.ProcessName, pi.ProcessId);
		auto node = m_Tree.InsertItem(text, icon, icon, TVI_ROOT, TVI_SORT);
		node.SetData((DWORD_PTR)ItemType::Process);
		m_Processes.insert({ node, pi });

		for (auto tid : pi.Threads) {
			text.Format(L"Thread %u (0x%X)", tid, tid);
			auto tnode = m_Tree.InsertItem(text, icon, icon, node, TVI_LAST);
			if (!WindowHelper::ThreadHasWindows(tid))
				tnode.SetState(TVIS_CUT, TVIS_CUT);
			tnode.SetData((DWORD_PTR)ItemType::Thread);
			m_Threads.insert({ tnode, tid });
			struct LocalData {
				CProcessesView* pThis;
				HTREEITEM hParent;
			};
			LocalData data = { this, tnode };
			::EnumThreadWindows(tid, [](auto hWnd, auto param) {
				auto data = (LocalData*)param;
				data->pThis->AddNode(hWnd, data->hParent);
				return TRUE;
				}, reinterpret_cast<LPARAM>(&data));

			//
			// add message-only windows
			//
			if (auto it = info.MessageOnly.find(tid); it != info.MessageOnly.end()) {
				for (auto hWnd : it->second) {
					AddNode(hWnd, tnode);
				}
			}
		}
	}
	m_Tree.LockWindowUpdate(FALSE);
}

CTreeItem CProcessesView::AddNode(HWND hWnd, HTREEITEM hParent) {
	CString text, name;
	CWindow win(hWnd);
	m_TotalWindows++;
	if (win.IsWindowVisible())
		m_TotalVisibleWindows++;

	m_TopLevelWindows++;

	if (!m_ShowHiddenWindows && !win.IsWindowVisible())
		return nullptr;
	win.GetWindowText(name);
	if (!m_ShowNoTitleWindows && name.IsEmpty())
		return nullptr;

	if (name.GetLength() > 64)
		name = name.Left(64) + L"...";
	if (!name.IsEmpty())
		name = L"[" + name + L"]";
	WCHAR className[64] = { 0 };
	::GetClassName(hWnd, className, _countof(className));
	text.Format(L"0x%zX (%s) %s", (DWORD_PTR)hWnd, className, (PCWSTR)name);

	HICON hIcon{ nullptr };
	int image = 0;
	if ((win.GetStyle() & WS_CHILD) == 0) {
		auto& icons = WindowHelper::GetIconMap();
		if (auto it = icons.find(hWnd); it == icons.end()) {
			hIcon = WindowHelper::GetWindowOrProcessIcon(hWnd);
			if (hIcon) {
				icons.insert({ hWnd, image = WindowHelper::GetImageList().AddIcon(hIcon) });
			}
		}
		else {
			image = it->second;
		}
	}

	auto node = m_Tree.InsertItem(text, image, image, hParent, TVI_LAST);
	node.SetData((DWORD_PTR)hWnd);
	m_WindowMap.insert({ hWnd, node });

	if (!win.IsWindowVisible())
		node.SetState(TVIS_CUT, TVIS_CUT);

	if (win.GetWindow(GW_CHILD)) {
		// add a "plus" button
		node.AddTail(L"*", 0);
	}
	return node;
}

LRESULT CProcessesView::OnNodeExpanding(int, LPNMHDR hdr, BOOL&) {
	auto tv = (NMTREEVIEW*)hdr;
	if (tv->action == TVE_EXPAND) {
		auto hItem = tv->itemNew.hItem;

		auto child = m_Tree.GetChildItem(hItem);
		if (child.GetData() == 0) {
			child.Delete();
			AddChildWindows(hItem);
		}
	}
	return 0;
}

void CProcessesView::AddChildWindows(HTREEITEM hParent) {
	auto hWnd = (HWND)m_Tree.GetItemData(hParent);
	ATLASSERT(hWnd);

	m_hCurrentNode = hParent;
	::EnumChildWindows(hWnd, [](auto hChild, auto p) -> BOOL {
		auto pThis = (CProcessesView*)p;
		return pThis->AddChildNode(hChild);
		}, reinterpret_cast<LPARAM>(this));
}

BOOL CProcessesView::AddChildNode(HWND hChild) {
	if (::GetAncestor(hChild, GA_PARENT) == (HWND)m_Tree.GetItemData(m_hCurrentNode)) {
		AddNode(hChild, m_hCurrentNode);
	}
	return TRUE;
}

LRESULT CProcessesView::OnTreeNodeRightClick(HTREEITEM hItem, CPoint const& pt) {
	ATLASSERT(m_Selected);

	auto data = static_cast<ItemType>(m_Selected.GetData());
	int index = 0;
	if (data == ItemType::Thread)
		index = 3;
	else if (data == ItemType::Process)
		index = 2;

	CMenu menu;
	menu.LoadMenu(IDR_CONTEXT);
	return GetFrame()->ShowPopupMenu(menu.GetSubMenu(index), pt);
}

LRESULT CProcessesView::OnNodeSelected(int, LPNMHDR hdr, BOOL&) {
	auto tv = (NMTREEVIEW*)hdr;
	if (tv->action == TVC_BYKEYBOARD) {
		// short delay before update in case the user moves quickly through the tree
		SetTimer(3, 250, nullptr);
	}
	else {
		ChangeSelection(tv->itemNew.hItem);
	}
	return 0;
}

LRESULT CProcessesView::OnTreeNodeDoubleClick(HTREEITEM hItem, CPoint const& pt) {
	auto data = m_Tree.GetItemData(hItem);
	switch (static_cast<ItemType>(data)) {
		case ItemType::Process:
			if (auto it = m_Processes.find(hItem); it != m_Processes.end())
				ProcessHelper::ShowProcessProperties(it->second, m_hWnd);
			break;

		case ItemType::Thread:
			break;

		default:
			auto hWnd = reinterpret_cast<HWND>(data);
			WindowHelper::ShowWindowProperties(hWnd);
			break;
	}
	return 0;
}

LRESULT CProcessesView::OnProcessProperties(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/) {
	if (auto pi = GetSelectedProcess())
		ProcessHelper::ShowProcessProperties(*pi, m_hWnd);
	return 0;
}

LRESULT CProcessesView::OnThreadProperties(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/) {
	if (auto tid = GetSelectedThread())
		ProcessHelper::ShowThreadProperties(tid, m_hWnd);
	return 0;
}

LRESULT CProcessesView::OnTerminateProcess(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/) {
	auto pi = GetSelectedProcess();
	if (!pi)
		return 0;

	CString text;
	text.Format(L"Terminate %s (%u)?", (PCWSTR)pi->ProcessName, pi->ProcessId);
	if (AtlMessageBox(m_hWnd, (PCWSTR)text, IDS_TITLE, MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2) != IDYES)
		return 0;

	if (!ProcessHelper::Terminate(pi->ProcessId)) {
		AtlMessageBox(m_hWnd, L"Failed to terminate the process", IDS_TITLE, MB_ICONERROR);
		return 0;
	}
	InitTree();
	return 0;
}

LRESULT CProcessesView::OnCaptureMessages(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& bHandled) {
	if (::GetFocus() != m_Tree) {
		bHandled = FALSE;		// the list's
		return 0;
	}

	auto data = static_cast<ItemType>(m_Selected.GetData());
	if (data == ItemType::Process) {
		if (auto pi = GetSelectedProcess())
			GetFrame()->CaptureMessages(nullptr, 0, pi->ProcessId);
	}
	else if (data == ItemType::Thread)
		GetFrame()->CaptureMessages(nullptr, GetSelectedThread());
	else if (m_SelectedHwnd)
		GetFrame()->CaptureMessages(m_SelectedHwnd);
	return 0;
}

ProcessInfo const* CProcessesView::GetSelectedProcess() const {
	for (auto hItem = m_Tree.GetSelectedItem(); hItem; hItem = m_Tree.GetParentItem(hItem))
		if (auto it = m_Processes.find(hItem); it != m_Processes.end())
			return &it->second;
	return nullptr;
}

DWORD CProcessesView::GetSelectedThread() const {
	for (auto hItem = m_Tree.GetSelectedItem(); hItem; hItem = m_Tree.GetParentItem(hItem))
		if (auto it = m_Threads.find(hItem); it != m_Threads.end())
			return it->second;
	return 0;
}

void CProcessesView::ChangeSelection(HTREEITEM hItem) {
	m_Selected = CTreeItem(hItem, &m_Tree);
	m_SelectedHwnd = nullptr;
	if (!hItem)
		return;

	auto data = static_cast<ItemType>(m_Selected.GetData());
	switch (data) {
		case ItemType::Process:
			if (auto it = m_Processes.find(hItem); it != m_Processes.end())
				m_WindowsView.UpdateListByProcess(it->second);
			break;

		case ItemType::Thread:
			if (auto it = m_Threads.find(hItem); it != m_Threads.end())
				m_WindowsView.UpdateListByThread(it->second);
			break;

		default:
			auto hWnd = (HWND)m_Selected.GetData();
			if (!::IsWindow(hWnd)) {	// window is probably destroyed
				RemoveFromWindowMap(hItem);
				m_Selected.Delete();
				m_Selected = CTreeItem();
			}
			else {
				m_SelectedHwnd = hWnd;
				m_WindowsView.UpdateList(hWnd);
			}
			break;
	}
}

void CProcessesView::RemoveFromWindowMap(HTREEITEM hItem) {
	// the node's window and its descendants'
	m_WindowMap.erase((HWND)m_Tree.GetItemData(hItem));
	for (auto child = m_Tree.GetChildItem(hItem); child; child = m_Tree.GetNextSiblingItem(child))
		RemoveFromWindowMap(child);
}

LRESULT CProcessesView::OnToggleHiddenWindows(WORD, WORD, HWND, BOOL&) {
	m_ShowHiddenWindows = !m_ShowHiddenWindows;
	UpdateUI();
	InitTree();
	return 0;
}

LRESULT CProcessesView::OnToggleEmptyTitleWindows(WORD, WORD, HWND, BOOL&) {
	m_ShowNoTitleWindows = !m_ShowNoTitleWindows;
	UpdateUI();
	InitTree();
	return 0;
}

void CProcessesView::UpdateUI() {
	auto& ui = GetFrame()->GetUIUpdate();
	if (::GetFocus() == m_Tree) {
		ui.UISetCheck(ID_VIEW_HIDDENWINDOWS, m_ShowHiddenWindows);
		ui.UISetCheck(ID_VIEW_EMPTYTITLEWINDOWS, m_ShowNoTitleWindows);
		ui.UIEnable(ID_WINDOW_PROPERTIES, m_SelectedHwnd != nullptr);
	}
	else {
		m_WindowsView.UpdateUI(ui);
	}
}
