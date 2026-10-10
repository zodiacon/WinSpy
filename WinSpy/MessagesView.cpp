#include "pch.h"
#include "resource.h"
#include "MessagesView.h"
#include "hooks.h"
#include "FormatHelper.h"
#include "WindowHelper.h"
#include "MessageDecoder.h"
#include "HookHelper.h"
#include "ProcessHelper.h"
#include "MessageFilterDlg.h"
#include "AppSettings.h"
#include <ListViewhelper.h>

bool CMessagesView::StartCapture() {
	StopCapture();
	if (!m_hReadyEvent)
		m_hReadyEvent = ::CreateEvent(nullptr, FALSE, FALSE, nullptr);

	m_hThread = ::CreateThread(nullptr, 0, [](auto param) {
		return ((CMessagesView*)param)->ProcessHook();
		}, this, 0, nullptr);
	if (!m_hThread)
		return false;

	ATLVERIFY(WAIT_OBJECT_0 == ::WaitForSingleObject(m_hReadyEvent, INFINITE));
	return m_CallbackWnd.IsWindow();
}

bool CMessagesView::AddHooks(DWORD tid, HWND hTarget, HookOptions options) {
	HookConfig config{};
	config.CallbackWnd = m_CallbackWnd;
	config.TargetWnd = hTarget;
	config.Options = options;
	config.ThreadId = tid;
	// posted messages are the essential ones; sent and returned ones only if there's room for them
	if (!HookHelper::AddHook(WH_GETMESSAGE, config))
		return false;
	HookHelper::AddHook(WH_CALLWNDPROC, config);
	HookHelper::AddHook(WH_CALLWNDPROCRET, config);
	UpdateHookFilter();
	return true;
}

void CMessagesView::UpdateHookFilter() {
	//
	// a pre-filter: the hooks send what the filter could show (the search text is applied here only)
	//
	static const struct {
		DWORD HookType;
		MessageKind Kind;
	} kinds[] = {
		{ WH_GETMESSAGE, MessageKind::Posted },
		{ WH_CALLWNDPROC, MessageKind::Sent },
		{ WH_CALLWNDPROCRET, MessageKind::Returned },
	};
	for (auto& k : kinds) {
		BYTE low[FilterLowMessages / 8]{};
		for (UINT msg = 0; msg < FilterLowMessages; msg++)
			if (m_Filter.IsVisible(k.Kind, msg))
				low[msg / 8] |= 1 << (msg % 8);

		// the messages above (all in the Other category) are let through if any of them can be shown;
		// the view filters them exactly
		bool kindShown = k.Kind == MessageKind::Posted ? m_Filter.Posted : k.Kind == MessageKind::Sent ? m_Filter.Sent : m_Filter.Returned;
		bool high = kindShown && (m_Filter.Include.empty()
			? (m_Filter.Categories & MessageCategory::Other) == MessageCategory::Other
			: std::any_of(m_Filter.Include.begin(), m_Filter.Include.end(), [](auto msg) { return msg >= FilterLowMessages; }));
		HookHelper::SetHookFilter(m_CallbackWnd, k.HookType, low, high);
	}
}

bool CMessagesView::CaptureWindow(HWND hWnd) {
	return StartCapture() && AddHooks(::GetWindowThreadProcessId(hWnd, nullptr), hWnd, HookOptions::Window | HookOptions::ChildWindows);
}

bool CMessagesView::CaptureThread(DWORD tid) {
	return StartCapture() && AddHooks(tid, nullptr, HookOptions::Thread);
}

bool CMessagesView::CaptureProcess(DWORD pid) {
	if (!StartCapture())
		return false;

	auto info = ProcessHelper::EnumProcessesAndThreads(EnumProcessesOptions::UIThreadsOnly | EnumProcessesOptions::IncludeMessageOnly);
	auto it = std::find_if(info.Processes.begin(), info.Processes.end(), [=](auto& pi) { return pi.ProcessId == pid; });
	if (it == info.Processes.end())
		return false;

	int hooked = 0;
	for (auto tid : it->Threads)
		if (AddHooks(tid, nullptr, HookOptions::Thread))
			hooked++;
	return hooked > 0;
}

void CMessagesView::StopCapture() {
	if (!m_hThread)
		return;

	//
	// no new callbacks once the hooks are gone; then wait for the callback thread to finish
	// the one it may be in, as it uses this object
	//
	HookHelper::RemoveHook(m_CallbackWnd);
	::PostThreadMessage(::GetThreadId(m_hThread), WM_QUIT, 0, 0);
	::WaitForSingleObject(m_hThread, INFINITE);
	::CloseHandle(m_hThread);
	m_hThread = nullptr;
}

void CMessagesView::HookCallbackMsg(HookDataHeader const* header) {
	if (m_Paused)
		return;

	MessageInfo mi{};
	switch (header->HookType) {
		case WH_GETMESSAGE:
		{
			auto data = reinterpret_cast<GetMessageData const*>(header);
			mi.Kind = MessageKind::Posted;
			mi.hWnd = data->Msg.hwnd;
			mi.Message = data->Msg.message;
			mi.wParam = data->Msg.wParam;
			mi.lParam = data->Msg.lParam;
			mi.Time = data->Msg.time;
			mi.pt = data->Msg.pt;
			break;
		}

		case WH_CALLWNDPROC:
		case WH_CALLWNDPROCRET:
		{
			auto data = reinterpret_cast<CallWndProcData const*>(header);
			mi.Kind = header->HookType == WH_CALLWNDPROC ? MessageKind::Sent : MessageKind::Returned;
			mi.hWnd = data->hWnd;
			mi.Message = data->Message;
			mi.wParam = data->wParam;
			mi.lParam = data->lParam;
			mi.Result = data->Result;
			mi.Time = data->Time;
			mi.FromOtherThread = data->FromOtherThread;
			mi.Extra = MessageDecoder::DecodeExtra(data->ExtraType, data->Extra, data->ExtraSize);
			break;
		}

		default:
			return;
	}
	mi.ThreadId = ::GetWindowThreadProcessId(mi.hWnd, &mi.ProcessId);

	std::lock_guard locker(m_TempMessagesLock);
	m_TempMessages.push_back(std::move(mi));
}

void CMessagesView::OnFinalMessage(HWND) {
	delete this;
}

CString CMessagesView::GetColumnText(HWND hWnd, int row, int col) const {
	auto& item = m_Messages[m_Visible[row]];
	CString text;
	switch (GetColumnManager(hWnd)->GetColumnTag<ColumnType>(col)) {
		case ColumnType::Type:
			switch (item.Kind) {
				case MessageKind::Posted: return L"Posted";
				case MessageKind::Sent: return item.FromOtherThread ? L"Sent (other thread)" : L"Sent";
				case MessageKind::Returned: return L"Returned";
			}
			break;

		case ColumnType::Window:
			return FormatHelper::FormatHWndOrNone(item.hWnd);

		case ColumnType::Message:
			text.Format(L"%s (0x%X)", (PCWSTR)WindowHelper::WindowMessageToString(item.Message), item.Message);
			break;

		case ColumnType::Thread:
			if (item.ThreadId)
				text.Format(L"%u", item.ThreadId);
			break;

		case ColumnType::Process:
			if (item.ProcessId)
				text.Format(L"%u", item.ProcessId);
			break;

		case ColumnType::ProcessName:
		{
			if (!item.ProcessId)
				break;
			// looked up once per process, not on every repaint
			auto it = m_ProcessNames.find(item.ProcessId);
			if (it == m_ProcessNames.end())
				it = m_ProcessNames.insert({ item.ProcessId, ProcessHelper::GetProcessImageName(item.ProcessId) }).first;
			return it->second;
		}

		case ColumnType::wParam:
			text.Format(L"0x%zX", item.wParam);
			break;

		case ColumnType::lParam:
			text.Format(L"0x%zX", item.lParam);
			break;

		case ColumnType::Result:
			if (item.Kind == MessageKind::Returned)
				text.Format(L"0x%zX", item.Result);
			break;

		case ColumnType::Time:
			text.Format(L"0x%X", item.Time);
			break;

		case ColumnType::Point:
			if (item.Kind == MessageKind::Posted)
				return FormatHelper::FormatPoint(item.pt);
			break;

		case ColumnType::DecodedMessage:
		{
			auto decoded = MessageDecoder::Decode(item.Message, item.wParam, item.lParam);
			if (item.Extra.IsEmpty())
				return decoded;
			return decoded.IsEmpty() ? item.Extra : decoded + L" " + item.Extra;
		}
	}
	return text;
}

void CMessagesView::UpdateList() {
	m_List.SetItemCountEx((int)m_Visible.size(), LVSICF_NOINVALIDATEALL | LVSICF_NOSCROLL);
	m_List.RedrawItems(m_List.GetTopIndex(), m_List.GetTopIndex() + m_List.GetCountPerPage());
}

bool CMessagesView::IsShown(MessageInfo const& mi) const {
	if (!m_Filter.IsVisible(mi.Kind, mi.Message))
		return false;
	if (m_SearchText.IsEmpty())
		return true;

	// the message's name, window and decoded text (m_SearchText is upper case)
	CString text;
	text.Format(L"%s 0x%zX ", (PCWSTR)WindowHelper::WindowMessageToString(mi.Message), (DWORD_PTR)mi.hWnd);
	text += MessageDecoder::Decode(mi.Message, mi.wParam, mi.lParam) + L" " + mi.Extra;
	return text.MakeUpper().Find(m_SearchText) >= 0;
}

void CMessagesView::ApplyFilter() {
	m_Visible.clear();
	for (int i = 0; i < (int)m_Messages.size(); i++)
		if (IsShown(m_Messages[i]))
			m_Visible.push_back(i);
	m_List.SetItemCountEx((int)m_Visible.size(), LVSICF_NOSCROLL);
	m_List.Invalidate();
}

DWORD CMessagesView::ProcessHook() {
	m_CallbackWnd.Create(HWND_MESSAGE, nullptr, nullptr);
	ATLASSERT(m_CallbackWnd);
	// when running elevated, the hooked (lower integrity) processes send to this window
	::ChangeWindowMessageFilterEx(m_CallbackWnd, WM_COPYDATA, MSGFLT_ALLOW, nullptr);

	::SetEvent(m_hReadyEvent);

	MSG msg;
	while (::GetMessage(&msg, nullptr, 0, 0))
		::DispatchMessage(&msg);

	m_CallbackWnd.DestroyWindow();
	return 0;
}

LRESULT CMessagesView::OnCreate(UINT, WPARAM, LPARAM, BOOL&) {
	ToolBarButtonInfo buttons[] = {
		{ ID_MESSAGES_FILTER, IDI_FILTER, BTNS_BUTTON, L"Filter..." },
		{ 0 },
		{ ID_MESSAGES_PAUSE, 0, BTNS_CHECK, L"Pause" },
		{ ID_MESSAGES_CLEAR, 0, BTNS_BUTTON, L"Clear" },
		{ ID_MESSAGES_AUTOSCROLL, 0, BTNS_CHECK, L"Auto Scroll" },
		{ 0 },
		{ ID_MESSAGES_SAVE, 0, BTNS_BUTTON, L"Save..." },
	};
	CToolBarCtrl tb(CreateAndInitToolBar(buttons, _countof(buttons)));
	UISetCheck(ID_MESSAGES_AUTOSCROLL, m_AutoScroll);

	// the search box, in a band of its own next to the toolbar
	CRect rc(0, 0, WindowHelper::DpiScale(220), WindowHelper::DpiScale(22));
	m_Search.Create(m_hWndToolBar, &rc, nullptr, WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, WS_EX_CLIENTEDGE, IDC_SEARCH);
	m_Search.SetFont(WindowHelper::GetUIFont());
	m_Search.SetCueBannerText(L"Search messages");
	AddSimpleReBarBand(m_Search, nullptr, FALSE, 220);

	// the toolbar's band as wide as all its buttons (otherwise the last ones go behind the chevron)
	SIZE size;
	tb.GetMaxSize(&size);
	CReBarCtrl rb(m_hWndToolBar);
	REBARBANDINFO rbbi{ sizeof(rbbi) };
	rbbi.fMask = RBBIM_CHILDSIZE;
	rb.GetBandInfo(0, &rbbi);
	rbbi.fMask = RBBIM_CHILDSIZE | RBBIM_SIZE | RBBIM_IDEALSIZE;
	rbbi.cxMinChild = rbbi.cx = rbbi.cxIdeal = size.cx;
	rb.SetBandInfo(0, &rbbi);

	m_hWndClient = m_List.Create(m_hWnd, rcDefault, nullptr, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN
		| LVS_REPORT | LVS_OWNERDATA | LVS_SHOWSELALWAYS | LVS_SHAREIMAGELISTS, WS_EX_CLIENTEDGE);
	m_List.SetExtendedListViewStyle(LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER | LVS_EX_INFOTIP);

	auto cm = GetColumnManager(m_List);

	struct {
		PCWSTR text;
		ColumnType type;
		int width = 100;
		int format = LVCFMT_LEFT;
		ColumnFlags flags = ColumnFlags::Visible;
	} columns[] = {
		{ L"Type", ColumnType::Type, 110 },
		{ L"Time", ColumnType::Time, 90, LVCFMT_LEFT },
		{ L"Window", ColumnType::Window, 100, LVCFMT_RIGHT },
		{ L"Message", ColumnType::Message, 180, LVCFMT_LEFT },
		{ L"WPARAM", ColumnType::wParam, 100, LVCFMT_RIGHT },
		{ L"LPARAM", ColumnType::lParam, 100, LVCFMT_RIGHT },
		{ L"Result", ColumnType::Result, 80, LVCFMT_RIGHT },
		{ L"Decoded Message", ColumnType::DecodedMessage, 350 },
		{ L"Thread", ColumnType::Thread, 70, LVCFMT_RIGHT },
		{ L"Process", ColumnType::Process, 70, LVCFMT_RIGHT },
		{ L"Process Name", ColumnType::ProcessName, 140 },
		{ L"Point", ColumnType::Point, 100, LVCFMT_RIGHT },
	};

	for (auto& col : columns) {
		cm->AddColumn(col.text, col.format, WindowHelper::DpiScale(col.width), col.type, col.flags);
	}

	cm->UpdateColumns();
	WindowHelper::SetColumnsLayout(m_List, AppSettings::Get().MessagesColumns().c_str());

	// the filter last chosen
	m_Filter.FromString(AppSettings::Get().MessageFilter().c_str());

	SetTimer(1, 1200);

	return 0;
}

LRESULT CMessagesView::OnTimer(UINT, WPARAM id, LPARAM, BOOL&) {
	if (id == 1) {
		{
			std::lock_guard locker(m_TempMessagesLock);
			if (m_TempMessages.empty())
				return 0;
			for (auto& mi : m_TempMessages) {
				if (IsShown(mi))
					m_Visible.push_back((int)m_Messages.size());
				m_Messages.push_back(std::move(mi));
			}
			m_TempMessages.clear();
		}
		if (m_Messages.size() > MaxMessages) {
			// drop the oldest tenth, so this doesn't happen on every update
			m_Messages.erase(m_Messages.begin(), m_Messages.begin() + (m_Messages.size() - MaxMessages * 9 / 10));
			ApplyFilter();
		}
		UpdateList();
		if (m_AutoScroll && !m_Visible.empty())
			m_List.EnsureVisible((int)m_Visible.size() - 1, FALSE);
	}
	else if (id == 2) {
		// the search text settled
		KillTimer(2);
		m_Search.GetWindowText(m_SearchText);
		m_SearchText.Trim();
		m_SearchText.MakeUpper();
		ApplyFilter();
	}
	return 0;
}

LRESULT CMessagesView::OnSearchChanged(WORD, WORD, HWND, BOOL&) {
	// filtering waits for a pause in the typing
	SetTimer(2, 300);
	return 0;
}

LRESULT CMessagesView::OnFind(WORD, WORD, HWND, BOOL&) {
	m_Search.SetFocus();
	m_Search.SetSelAll();
	return 0;
}

LRESULT CMessagesView::OnPause(WORD, WORD, HWND, BOOL&) {
	m_Paused = !m_Paused;
	UISetCheck(ID_MESSAGES_PAUSE, m_Paused.load());
	return 0;
}

LRESULT CMessagesView::OnClear(WORD, WORD, HWND, BOOL&) {
	{
		std::lock_guard locker(m_TempMessagesLock);
		m_TempMessages.clear();
	}
	m_Messages.clear();
	m_Visible.clear();
	m_List.SetItemCount(0);
	return 0;
}

LRESULT CMessagesView::OnAutoScroll(WORD, WORD, HWND, BOOL&) {
	m_AutoScroll = !m_AutoScroll;
	UISetCheck(ID_MESSAGES_AUTOSCROLL, m_AutoScroll);
	return 0;
}

LRESULT CMessagesView::OnSave(WORD, WORD, HWND, BOOL&) {
	// the rows that are shown, as they are shown
	auto path = ListViewHelper::PromptForCsvFile(m_hWnd, L"Messages");
	if (path.IsEmpty())
		return 0;
	if (!ListViewHelper::SaveAsCsv(m_List, path))
		AtlMessageBox(m_hWnd, L"Failed to save the file", IDS_TITLE, MB_ICONERROR);
	return 0;
}

LRESULT CMessagesView::OnFilter(WORD, WORD, HWND, BOOL&) {
	CMessageFilterDlg dlg(m_Filter);
	if (dlg.DoModal(m_hWnd) == IDOK) {
		m_Filter = dlg.GetFilter();
		AppSettings::Get().MessageFilter(std::wstring(m_Filter.ToString()));
		UpdateHookFilter();
		ApplyFilter();
	}
	return 0;
}

LRESULT CMessagesView::OnDestroy(UINT, WPARAM, LPARAM, BOOL& bHandled) {
	AppSettings::Get().MessagesColumns(std::wstring(WindowHelper::GetColumnsLayout(m_List)));
	StopCapture();
	if (m_hReadyEvent)
		::CloseHandle(m_hReadyEvent);

	// let CViewBase clean up as well
	bHandled = FALSE;
	return 0;
}

LRESULT CHookCallbackWnd::OnHookCallback(UINT msg, WPARAM wp, LPARAM lp, BOOL&) {
	auto cds = reinterpret_cast<COPYDATASTRUCT*>(lp);
	// any process can send this window WM_COPYDATA
	if (!cds || !cds->lpData || cds->cbData < sizeof(HookDataHeader))
		return FALSE;

	auto header = reinterpret_cast<HookDataHeader const*>(cds->lpData);
	switch (header->HookType) {
		case WH_GETMESSAGE:
			if (cds->cbData < sizeof(GetMessageData))
				return FALSE;
			break;

		case WH_CALLWNDPROC:
		case WH_CALLWNDPROCRET:
		{
			auto data = reinterpret_cast<CallWndProcData const*>(header);
			if (cds->cbData < offsetof(CallWndProcData, Extra) || data->ExtraSize > MaxExtraData
				|| cds->cbData < offsetof(CallWndProcData, Extra) + data->ExtraSize)
				return FALSE;
			break;
		}

		default:
			return FALSE;
	}
	m_pView->HookCallbackMsg(header);
	return TRUE;
}
