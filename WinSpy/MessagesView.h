#pragma once

#include "ViewBase.h"
#include "VirtualListView.h"
#include "hooks.h"

struct CMessagesView;

struct CHookCallbackWnd : CWindowImpl<CHookCallbackWnd> {
	CHookCallbackWnd(CMessagesView* pView) : m_pView(pView) {}

	BEGIN_MSG_MAP(CHookCallbackWnd)
		MESSAGE_HANDLER(WM_COPYDATA, OnHookCallback)
	END_MSG_MAP()

	LRESULT OnHookCallback(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);

private:
	CMessagesView* m_pView;
};

struct CMessagesView : CViewBase<CMessagesView>, CVirtualListView<CMessagesView> {
	CMessagesView(IMainFrame* frame) : CViewBase(frame), m_CallbackWnd(this) {}

	bool CaptureWindow(HWND hWnd);
	bool CaptureThread(DWORD tid);
	bool CaptureProcess(DWORD tid);

	void HookCallbackMsg(DWORD hookType, HookDataHeader* header);

	void OnFinalMessage(HWND) override;

	CString GetColumnText(HWND, int row, int col) const;

protected:
	BEGIN_MSG_MAP(CMessagesView)
		MESSAGE_HANDLER(WM_TIMER, OnTimer)
		MESSAGE_HANDLER(WM_CREATE, OnCreate)
		MESSAGE_HANDLER(WM_DESTROY, OnDestroy)
		CHAIN_MSG_MAP(CVirtualListView<CMessagesView>)
		CHAIN_MSG_MAP(CViewBase<CMessagesView>)
	END_MSG_MAP()

private:
	enum class ColumnType {
		Type,
		Window,
		Thread,
		Process,
		Message,
		DecodedMessage,
		wParam,
		lParam,
		Time,
		Point
	};

	enum class CaptureOptions {
		Window,
		Thread,
		Process
	};

	enum class MessageInfoFlags {
		None = 0,
		Result = 1,
	};

	struct MessageInfo : MSG {
		MessageInfoFlags Flags;	
		DWORD ThreadId;
		DWORD ProcessId;
	};

	void UpdateUI();
	void UpdateList();
	DWORD ProcessHook();
	LRESULT OnCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnTimer(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnDestroy(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);

	CListViewCtrl m_List;
	std::vector<MessageInfo> m_Messages;
	std::vector<MessageInfo> m_TempMessages;
	std::mutex m_TempMessagesLock;
	CaptureOptions m_CaptureOptions;
	DWORD m_CaptureTid;
	HWND m_CaptureHwnd;
	HANDLE m_hThread{ nullptr };
	CHookCallbackWnd m_CallbackWnd;
	HANDLE m_hReadyEvent{ nullptr };
};
