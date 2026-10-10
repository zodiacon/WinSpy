#pragma once

#include "ViewBase.h"
#include "VirtualListView.h"
#include "hooks.h"
#include "MessageFilter.h"
#include <atomic>

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
	bool CaptureProcess(DWORD pid);

	// called on the callback thread, with data that has been validated
	void HookCallbackMsg(HookDataHeader const* header);

	void OnFinalMessage(HWND) override;

	CString GetColumnText(HWND, int row, int col) const;

protected:
	BEGIN_MSG_MAP(CMessagesView)
		MESSAGE_HANDLER(WM_TIMER, OnTimer)
		MESSAGE_HANDLER(WM_CREATE, OnCreate)
		MESSAGE_HANDLER(WM_DESTROY, OnDestroy)
		COMMAND_ID_HANDLER(ID_MESSAGES_FILTER, OnFilter)
		COMMAND_ID_HANDLER(ID_MESSAGES_PAUSE, OnPause)
		COMMAND_ID_HANDLER(ID_MESSAGES_CLEAR, OnClear)
		COMMAND_ID_HANDLER(ID_MESSAGES_AUTOSCROLL, OnAutoScroll)
		COMMAND_ID_HANDLER(ID_MESSAGES_SAVE, OnSave)
		COMMAND_HANDLER(IDC_SEARCH, EN_CHANGE, OnSearchChanged)
		CHAIN_MSG_MAP(CVirtualListView<CMessagesView>)
		CHAIN_MSG_MAP(CViewBase<CMessagesView>)
	END_MSG_MAP()

private:
	enum class ColumnType {
		Type,
		Window,
		Thread,
		Process,
		ProcessName,
		Message,
		DecodedMessage,
		wParam,
		lParam,
		Result,
		Time,
		Point
	};

	struct MessageInfo {
		MessageKind Kind;
		HWND hWnd;
		UINT Message;
		WPARAM wParam;
		LPARAM lParam;
		LRESULT Result;			// returned messages
		DWORD Time;
		POINT pt;				// posted messages
		bool FromOtherThread;
		DWORD ThreadId;
		DWORD ProcessId;
		CString Extra;			// what a sent message's pointer parameter pointed to
	};

	void UpdateList();
	// starts the thread with the window the hooks send to (stopping a previous capture)
	bool StartCapture();
	// the posted, sent and returned message hooks of a thread
	bool AddHooks(DWORD tid, HWND hTarget, HookOptions options);
	DWORD ProcessHook();
	void StopCapture();
	void ApplyFilter();
	// the filter and the search text
	bool IsShown(MessageInfo const& mi) const;
	// gives the hooks the filter, so messages it hides aren't sent at all
	void UpdateHookFilter();

	LRESULT OnCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnTimer(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnDestroy(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnFilter(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnPause(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnClear(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnAutoScroll(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnSave(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnSearchChanged(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);

	// the oldest messages are dropped beyond this
	static constexpr size_t MaxMessages = 100000;

	CListViewCtrl m_List;
	std::vector<MessageInfo> m_Messages;
	std::vector<int> m_Visible;			// indices into m_Messages of the messages the filter lets through
	std::vector<MessageInfo> m_TempMessages;
	std::mutex m_TempMessagesLock;
	mutable std::unordered_map<DWORD, CString> m_ProcessNames;
	MessageFilter m_Filter;
	CString m_SearchText;
	CEdit m_Search;
	std::atomic<bool> m_Paused{ false };		// read on the callback thread
	bool m_AutoScroll{ true };
	HANDLE m_hThread{ nullptr };
	CHookCallbackWnd m_CallbackWnd;
	HANDLE m_hReadyEvent{ nullptr };
};
