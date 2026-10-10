#include "pch.h"
#include "resource.h"
#include "ProcessesView.h"
#include "ImageIconCache.h"
#include "AppSettings.h"

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
	m_Splitter.SetSplitterPosPct(AppSettings::Get().ProcessesSplitter());

	m_ShowHiddenWindows = AppSettings::Get().ProcessesShowHidden() != 0;
	m_ShowNoTitleWindows = AppSettings::Get().ProcessesShowNoTitle() != 0;
	InitTree();

	WindowEvents::Subscribe(this);

	return 0;
}

LRESULT CProcessesView::OnDestroy(UINT, WPARAM, LPARAM, BOOL& bHandled) {
	WindowEvents::Unsubscribe(this);
	AppSettings::Get().ProcessesSplitter(m_Splitter.GetSplitterPosPct());
	bHandled = FALSE;
	return 0;
}

void CProcessesView::OnActivate(bool active) {
	if (active) {
		UpdateUI();
	}
}


LRESULT CProcessesView::OnTimer(UINT /*uMsg*/, WPARAM id, LPARAM /*lParam*/, BOOL& /*bHandled*/) {
	switch (id) {
		case TimerKeyboardSelect:
			KillTimer(id);
			ChangeSelection(m_Tree.GetSelectedItem());
			break;

		case TimerEvents:
			KillTimer(id);
			ProcessWindowEvents();
			break;
	}
	return 0;
}

LRESULT CProcessesView::OnRefresh(WORD, WORD, HWND, BOOL&) {
	InitTree();
	return 0;
}

void CProcessesView::InitTree() {
	WindowHelper::PruneIconMap();
	m_Tree.LockWindowUpdate();
	m_Deleting = true;
	m_Tree.DeleteAllItems();
	m_Deleting = false;
	m_Processes.clear();
	m_WindowMap.clear();
	m_Threads.clear();
	m_ProcessNodes.clear();
	m_ThreadNodes.clear();
	m_Selected = CTreeItem();
	m_SelectedHwnd = nullptr;
	m_hCurrentNode = CTreeItem();
	m_TotalWindows = m_TotalVisibleWindows = m_TopLevelWindows = 0;

	auto info = ProcessHelper::EnumProcessesAndThreads(
		EnumProcessesOptions::UIThreadsOnly | EnumProcessesOptions::SkipProcessesWithNoUI | EnumProcessesOptions::IncludeMessageOnly);
	for (auto& pi : info.Processes) {
		auto node = AddProcessNode(pi);
		int icon;
		node.GetImage(icon, icon);

		for (auto tid : pi.Threads) {
			auto tnode = AddThreadNode(node, pi.ProcessId, tid, icon);
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

CTreeItem CProcessesView::AddProcessNode(ProcessInfo const& pi) {
	auto icon = ImageIconCache::Get().GetIcon(pi.FullPath);
	CString text;
	text.Format(L"%s (%u)", (PCWSTR)pi.ProcessName, pi.ProcessId);
	if (!pi.Accessible)
		text += L" - access denied";
	auto node = m_Tree.InsertItem(text, icon, icon, TVI_ROOT, TVI_SORT);
	node.SetData((DWORD_PTR)ItemType::Process);
	m_Processes.insert({ node, pi });
	m_ProcessNodes.insert({ pi.ProcessId, node });
	return node;
}

CTreeItem CProcessesView::AddThreadNode(HTREEITEM hProcess, DWORD pid, DWORD tid, int icon) {
	CString text;
	text.Format(L"Thread %u (0x%X)", tid, tid);
	auto node = m_Tree.InsertItem(text, icon, icon, hProcess, TVI_LAST);
	if (!WindowHelper::ThreadHasWindows(tid))
		node.SetState(TVIS_CUT, TVIS_CUT);
	node.SetData((DWORD_PTR)ItemType::Thread);
	m_Threads.insert({ node, tid });
	m_ThreadNodes.insert({ tid, { node, pid } });
	// the process knows its UI threads (for the list of its windows)
	if (auto it = m_Processes.find(hProcess); it != m_Processes.end() && std::find(it->second.Threads.begin(), it->second.Threads.end(), tid) == it->second.Threads.end())
		it->second.Threads.push_back(tid);
	return node;
}

HTREEITEM CProcessesView::EnsureThreadNode(DWORD pid, DWORD tid) {
	if (auto it = m_ThreadNodes.find(tid); it != m_ThreadNodes.end())
		return it->second.Node;

	HTREEITEM hProcess;
	if (auto it = m_ProcessNodes.find(pid); it != m_ProcessNodes.end())
		hProcess = it->second;
	else {
		ProcessInfo pi;
		pi.ProcessId = pid;
		pi.ProcessName = ProcessHelper::GetProcessImageName(pid);
		pi.FullPath = ProcessHelper::GetProcessImageName(pid, true);
		auto hProcessHandle = ::OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
		pi.Accessible = hProcessHandle != nullptr;
		if (hProcessHandle)
			::CloseHandle(hProcessHandle);
		hProcess = AddProcessNode(pi);
	}
	int icon;
	CTreeItem(hProcess, &m_Tree).GetImage(icon, icon);
	return AddThreadNode(hProcess, pid, tid, icon);
}

bool CProcessesView::PassesFilter(HWND hWnd) const {
	return (m_ShowHiddenWindows || ::IsWindowVisible(hWnd)) && (m_ShowNoTitleWindows || ::GetWindowTextLength(hWnd) > 0);
}

CTreeItem CProcessesView::AddNode(HWND hWnd, HTREEITEM hParent) {
	CString text, name;
	CWindow win(hWnd);
	m_TotalWindows++;
	if (win.IsWindowVisible())
		m_TotalVisibleWindows++;

	m_TopLevelWindows++;

	if (!PassesFilter(hWnd))
		return nullptr;
	win.GetWindowText(name);

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
		SetTimer(TimerKeyboardSelect, 250, nullptr);
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

LRESULT CProcessesView::OnNodeDeleted(int, LPNMHDR hdr, BOOL&) {
	// the maps follow the tree (except when it's cleared, and they with it)
	if (m_Deleting)
		return 0;

	auto& item = reinterpret_cast<NMTREEVIEW*>(hdr)->itemOld;
	auto hItem = item.hItem;
	if (auto it = m_Processes.find(hItem); it != m_Processes.end()) {
		m_ProcessNodes.erase(it->second.ProcessId);
		m_Processes.erase(it);
	}
	else if (auto it = m_Threads.find(hItem); it != m_Threads.end()) {
		auto tid = it->second;
		if (auto tn = m_ThreadNodes.find(tid); tn != m_ThreadNodes.end()) {
			// the process doesn't count it among its UI threads anymore
			if (auto pn = m_ProcessNodes.find(tn->second.ProcessId); pn != m_ProcessNodes.end())
				if (auto pi = m_Processes.find(pn->second); pi != m_Processes.end())
					std::erase(pi->second.Threads, tid);
			m_ThreadNodes.erase(tn);
		}
		m_Threads.erase(it);
	}
	else if (auto hWnd = (HWND)item.lParam; hWnd) {
		if (auto it = m_WindowMap.find(hWnd); it != m_WindowMap.end() && it->second == hItem)
			m_WindowMap.erase(it);
	}
	return 0;
}

void CProcessesView::OnWindowEvent(DWORD event, HWND hWnd) {
	if (event == EVENT_OBJECT_DESTROY)
		m_EventsDestroyed.insert(hWnd);
	else
		m_EventsChanged.insert(hWnd);
	// handled together a little later, as windows come and go in bursts (the first event of a batch sets the timer)
	if (m_EventsDestroyed.size() + m_EventsChanged.size() == 1)
		SetTimer(TimerEvents, 250);
}

DWORD CProcessesView::GetNodeThread(HTREEITEM hItem) const {
	for (; hItem; hItem = m_Tree.GetParentItem(hItem))
		if (auto it = m_Threads.find(hItem); it != m_Threads.end())
			return it->second;
	return 0;
}

void CProcessesView::ProcessWindowEvents() {
	auto destroyed = std::move(m_EventsDestroyed);
	auto changed = std::move(m_EventsChanged);
	m_EventsDestroyed.clear();
	m_EventsChanged.clear();

	// does the list show this window? (a selected window's children, or a selected thread's or process's windows)
	DWORD selectedTid = 0, selectedPid = 0;
	if (!m_SelectedHwnd && m_Selected) {
		auto type = static_cast<ItemType>(m_Selected.GetData());
		if (type == ItemType::Thread)
			selectedTid = GetSelectedThread();
		else if (auto pi = GetSelectedProcess(); type == ItemType::Process && pi)
			selectedPid = pi->ProcessId;
	}
	bool listChanged = false;
	auto inList = [&](HWND hWnd) {
		if (m_WindowsView.IsListed(hWnd))
			return true;
		if (!::IsWindow(hWnd))
			return false;
		if (m_SelectedHwnd)
			return ::GetAncestor(hWnd, GA_PARENT) == m_SelectedHwnd.m_hWnd;
		DWORD pid;
		auto tid = ::GetWindowThreadProcessId(hWnd, &pid);
		return (selectedTid && tid == selectedTid) || (selectedPid && pid == selectedPid);
	};

	// the threads that lost windows
	std::unordered_set<DWORD> threads;
	for (auto hWnd : destroyed) {
		listChanged = listChanged || inList(hWnd);
		if (auto it = m_WindowMap.find(hWnd); it != m_WindowMap.end()) {
			threads.insert(GetNodeThread(it->second));
			m_Tree.DeleteItem(it->second);
		}
	}
	for (auto hWnd : changed) {
		if (destroyed.contains(hWnd))
			continue;
		listChanged = listChanged || inList(hWnd);
		UpdateWindowNode(hWnd, threads);
	}
	for (auto tid : threads)
		if (tid)
			RemoveThreadIfEmpty(tid);

	if (listChanged)
		m_WindowsView.Refresh();
}

void CProcessesView::UpdateWindowNode(HWND hWnd, std::unordered_set<DWORD>& threads) {
	auto it = m_WindowMap.find(hWnd);
	if (!::IsWindow(hWnd)) {
		if (it != m_WindowMap.end()) {
			threads.insert(GetNodeThread(it->second));
			m_Tree.DeleteItem(it->second);
		}
		return;
	}

	auto hParent = ::GetAncestor(hWnd, GA_PARENT);
	bool topLevel = hParent == ::GetDesktopWindow();
	if (it != m_WindowMap.end()) {
		CTreeItem node(it->second, &m_Tree);
		// a top-level window is under its thread's node, a child window under its parent's
		auto parentNode = node.GetParent();
		bool inPlace = topLevel ? m_Threads.contains(parentNode) : (HWND)parentNode.GetData() == hParent;
		if (inPlace && PassesFilter(hWnd)) {
			CString name, text;
			CWindow(hWnd).GetWindowText(name);
			if (name.GetLength() > 64)
				name = name.Left(64) + L"...";
			if (!name.IsEmpty())
				name = L"[" + name + L"]";
			text.Format(L"0x%zX (%s) %s", (DWORD_PTR)hWnd, (PCWSTR)WindowHelper::GetWindowClassName(hWnd), (PCWSTR)name);
			node.SetText(text);
			node.SetState(::IsWindowVisible(hWnd) ? 0 : TVIS_CUT, TVIS_CUT);
			return;
		}
		threads.insert(GetNodeThread(node));
		node.Delete();
		if (inPlace)
			return;		// left out by the View filters
	}

	if (topLevel) {
		// under its thread (and process), which may be new
		DWORD pid;
		auto tid = ::GetWindowThreadProcessId(hWnd, &pid);
		if (tid)
			AddNode(hWnd, EnsureThreadNode(pid, tid));
		return;
	}

	// a child window: under its parent's node, if that node's children are there
	auto parent = m_WindowMap.find(hParent);
	if (parent == m_WindowMap.end())
		return;
	CTreeItem parentNode(parent->second, &m_Tree);
	auto child = parentNode.GetChild();
	if (child && child.GetData() == 0)
		return;		// added when the node is expanded
	if (!child) {
		// its first child: now it gets a "plus" button
		parentNode.AddTail(L"*", 0);
		return;
	}
	AddNode(hWnd, parentNode);
}

void CProcessesView::RemoveThreadIfEmpty(DWORD tid) {
	auto it = m_ThreadNodes.find(tid);
	if (it == m_ThreadNodes.end())
		return;
	CTreeItem thread(it->second.Node, &m_Tree);
	if (thread.GetChild() || WindowHelper::ThreadHasWindows(tid))
		return;		// it still has windows (maybe left out by the View filters)

	auto process = thread.GetParent();
	thread.Delete();
	if (process && !process.GetChild())
		process.Delete();
}

LRESULT CProcessesView::OnToggleHiddenWindows(WORD, WORD, HWND, BOOL&) {
	m_ShowHiddenWindows = !m_ShowHiddenWindows;
	AppSettings::Get().ProcessesShowHidden(m_ShowHiddenWindows);
	UpdateUI();
	InitTree();
	return 0;
}

LRESULT CProcessesView::OnToggleEmptyTitleWindows(WORD, WORD, HWND, BOOL&) {
	m_ShowNoTitleWindows = !m_ShowNoTitleWindows;
	AppSettings::Get().ProcessesShowNoTitle(m_ShowNoTitleWindows);
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
