#include "pch.h"
#include "MessagesView.h"
#include "hooks.h"
#include "FormatHelper.h"
#include "WindowHelper.h"
#include "MessageDecoder.h"

bool CMessagesView::CaptureWindow(HWND hWnd) {
	if (!m_hReadyEvent)
		m_hReadyEvent = ::CreateEvent(nullptr, FALSE, FALSE, nullptr);

	m_CaptureHwnd = hWnd;
	m_CaptureOptions = CaptureOptions::Window;

	m_CaptureTid = ::GetWindowThreadProcessId(hWnd, nullptr);
	if (m_hThread) {
		::PostThreadMessage(::GetThreadId(m_hThread), WM_QUIT, 0, 0);
		::CloseHandle(m_hThread);
	}

	m_hThread = ::CreateThread(nullptr, 0, [](auto param) {
		return ((CMessagesView*)param)->ProcessHook();
		}, this, 0, nullptr);
	if (!m_hThread)
		return false;

	ATLVERIFY(WAIT_OBJECT_0 == ::WaitForSingleObject(m_hReadyEvent, INFINITE));
	ATLASSERT(m_CallbackWnd);

	HookConfig config{};
	config.CallbackWnd = m_CallbackWnd;
	config.TargetWnd = hWnd;
	config.Options = HookOptions::Window | HookOptions::ChildWindows;
	config.ThreadId = m_CaptureTid;
	return AddHook(WH_GETMESSAGE, config);
}

void CMessagesView::HookCallbackMsg(DWORD hookType, HookDataHeader* header) {
	switch (header->HookType) {
		case WH_GETMESSAGE:
			auto data = reinterpret_cast<GetMessageData*>(header);
			if (data->wParam == PM_REMOVE) {
				MessageInfo mi(data->Msg);
				mi.ThreadId = ::GetWindowThreadProcessId(mi.hwnd, &mi.ProcessId);
				std::lock_guard locker(m_TempMessagesLock);
				m_TempMessages.push_back(mi);
			}
			break;
	}
}

void CMessagesView::OnFinalMessage(HWND) {
	delete this;
}

CString CMessagesView::GetColumnText(HWND hWnd, int row, int col) const {
	auto& item = m_Messages[row];
	CString text;
	switch (GetColumnManager(hWnd)->GetColumnTag<ColumnType>(col)) {
		case ColumnType::Window: 
			return FormatHelper::FormatHWndOrNone(item.hwnd);

		case ColumnType::Message: 
			text.Format(L"%s (0x%X)", (PCWSTR)WindowHelper::WindowMessageToString(item.message), item.message);
			break;

		case ColumnType::Thread:
			text.Format(L"%u", item.ThreadId);
			break;

		case ColumnType::Process:
			text.Format(L"%u", item.ProcessId);
			break;

		case ColumnType::wParam:
			text.Format(L"0x%zX", item.wParam);
			break;

		case ColumnType::lParam:
			text.Format(L"0x%zX", item.lParam);
			break;

		case ColumnType::Time:
			text.Format(L"0x%X", item.time);
			break;

		case ColumnType::Point:
			return FormatHelper::FormatPoint(item.pt);

		case ColumnType::DecodedMessage:
			return MessageDecoder::Decode(item.message, item.wParam, item.lParam);
	}
	return text;
}

void CMessagesView::UpdateList() {
	m_List.SetItemCountEx((int)m_Messages.size(), LVSICF_NOINVALIDATEALL | LVSICF_NOSCROLL);
	m_List.RedrawItems(m_List.GetTopIndex(), m_List.GetTopIndex() + m_List.GetCountPerPage());
}

DWORD CMessagesView::ProcessHook() {
	m_CallbackWnd.Create(HWND_MESSAGE, nullptr, nullptr);
	ATLASSERT(m_CallbackWnd);

	::SetEvent(m_hReadyEvent);

	MSG msg;
	while (::GetMessage(&msg, nullptr, 0, 0))
		::DispatchMessage(&msg);

	m_CallbackWnd.DestroyWindow();
	return 0;
}

LRESULT CMessagesView::OnCreate(UINT, WPARAM, LPARAM, BOOL&) {
	m_hWndClient = m_List.Create(m_hWnd, rcDefault, nullptr, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN
		| LVS_REPORT | LVS_OWNERDATA | LVS_SINGLESEL | LVS_SHAREIMAGELISTS, WS_EX_CLIENTEDGE);
	m_List.SetExtendedListViewStyle(LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER | LVS_EX_INFOTIP);
	
	auto cm = GetColumnManager(m_List);

	struct {
		PCWSTR text;
		ColumnType type;
		int width = 100;
		int format = LVCFMT_LEFT;
		ColumnFlags flags = ColumnFlags::Visible;
	} columns[] = {
//		{ L"Type", ColumnType::Type, 70 },
		{ L"Time", ColumnType::Time, 120, LVCFMT_LEFT },
		{ L"Window", ColumnType::Window, 100, LVCFMT_RIGHT },
		{ L"Message", ColumnType::Message, 140, LVCFMT_LEFT },
		{ L"WPARAM", ColumnType::wParam, 100, LVCFMT_RIGHT },
		{ L"LPARAM", ColumnType::lParam, 100, LVCFMT_RIGHT },
		{ L"Thread", ColumnType::Thread, 100, LVCFMT_RIGHT },
		{ L"Decoded Message", ColumnType::DecodedMessage, 250 },
		{ L"Process", ColumnType::Process, 100, LVCFMT_RIGHT },
		{ L"Point", ColumnType::Point, 100, LVCFMT_RIGHT },
	};

	for (auto& col : columns) {
		cm->AddColumn(col.text, col.format, col.width, col.type, col.flags);
	}

	cm->UpdateColumns();

	SetTimer(1, 1200);

	return 0;
}

LRESULT CMessagesView::OnTimer(UINT, WPARAM id, LPARAM, BOOL&) {
	if (id == 1) {
		{
			std::lock_guard locker(m_TempMessagesLock);
			if (m_TempMessages.empty())
				return 0;
			m_Messages.insert(m_Messages.end(), m_TempMessages.begin(), m_TempMessages.end());
			m_TempMessages.clear();
		}
		UpdateList();
	}
	return 0;
}

LRESULT CMessagesView::OnDestroy(UINT, WPARAM, LPARAM, BOOL&) {
	if (m_hThread) {
		::PostThreadMessage(::GetThreadId(m_hThread), WM_QUIT, 0, 0);
		::CloseHandle(m_hThread);
		RemoveHook(m_CaptureTid);
	}
	if (m_hReadyEvent)
		::CloseHandle(m_hReadyEvent);

	return LRESULT();
}

LRESULT CHookCallbackWnd::OnHookCallback(UINT msg, WPARAM wp, LPARAM lp, BOOL&) {
	auto cds = reinterpret_cast<COPYDATASTRUCT*>(lp);
	auto header = reinterpret_cast<HookDataHeader*>(cds->lpData);
	m_pView->HookCallbackMsg(header->HookType, header);
	return TRUE;
}
