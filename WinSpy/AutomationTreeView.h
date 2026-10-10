#pragma once

#include "resource.h"
#include "ViewBase.h"
#include "WindowCommands.h"
#include <VirtualListView.h>
#include <CustomSplitterWindow.h>
#include <UIAutomation.h>

class CAutomationEventHandler;

class CAutomationTreeView :
	public CViewBase<CAutomationTreeView>,
	public CVirtualListView<CAutomationTreeView> {
public:
	explicit CAutomationTreeView(IMainFrame* frame) : CViewBase(frame) {}

	void OnActivate(bool active);

	CString GetColumnText(HWND, int row, int col) const;

	// posted by the events handler when it has noted changes (after there were none)
	static constexpr UINT WM_AUTOMATION_EVENTS = WM_APP + 200;

protected:
	enum { IDC_TREE = 123 };
	enum { TimerEvents = 1 };

	BEGIN_MSG_MAP(CAutomationTreeView)
		MESSAGE_HANDLER(WM_CREATE, OnCreate)
		MESSAGE_HANDLER(WM_DESTROY, OnDestroy)
		MESSAGE_HANDLER(WM_LOCATE_ELEMENT, OnLocateElement)
		MESSAGE_HANDLER(WM_AUTOMATION_EVENTS, OnAutomationEvents)
		MESSAGE_HANDLER(WM_TIMER, OnTimer)
		NOTIFY_CODE_HANDLER(TVN_ITEMEXPANDING, OnNodeExpanding)
		NOTIFY_CODE_HANDLER(TVN_ITEMEXPANDED, OnNodeExpanded)
		NOTIFY_CODE_HANDLER(TVN_SELCHANGED, OnNodeSelected)
		NOTIFY_CODE_HANDLER(TVN_DELETEITEM, OnNodeDeleted)
		NOTIFY_HANDLER(IDC_TREE, NM_RCLICK, OnTreeRightClick)
		COMMAND_ID_HANDLER(ID_VIEW_REFRESH, OnRefresh)
		COMMAND_ID_HANDLER(ID_WINDOW_HIGHLIGHT, OnHighlight)
		COMMAND_ID_HANDLER(ID_AUTOMATION_LIVE, OnToggleLive)
		COMMAND_RANGE_HANDLER(ID_AUTOMATION_INVOKE, ID_AUTOMATION_SCROLLINTOVIEW, OnElementAction)
		// the other window commands act on the element's window (or its nearest ancestor's)
		if (uMsg == WM_COMMAND && WindowCommands::IsWindowCommand(LOWORD(wParam))) {
			WindowCommands::Execute(LOWORD(wParam), GetElementWindow(GetSelectedElement()), GetFrame());
			return TRUE;
		}
		CHAIN_MSG_MAP(CVirtualListView<CAutomationTreeView>)
		CHAIN_MSG_MAP(CViewBase<CAutomationTreeView>)
	END_MSG_MAP()

private:
	HTREEITEM AddElement(IUIAutomationElement* element, HTREEITEM hParent = TVI_ROOT, HTREEITEM hAfter = TVI_SORT);
	struct ChildElement {
		std::wstring Key;		// the runtime ID
		CComPtr<IUIAutomationElement> Element;
		CString Text;
	};
	// the element's children (but WinSpy's own) with what their nodes need, fetched together (a cache request)
	std::vector<ChildElement> GetChildElements(IUIAutomationElement* element);
	// a child node, with a "+" if the element has children
	HTREEITEM AddChildElement(ChildElement const& child, HTREEITEM hParent);
	void SetHasChildren(HTREEITEM hItem, bool children);
	static CString GetElementText(IUIAutomationElement* element);
	void EnumChildElements(IUIAutomationElement* root, HTREEITEM hParent);
	// the child node of the element (its children are enumerated if they have not been yet, or again if it is not among them)
	HTREEITEM FindChildNode(HTREEITEM hParent, IUIAutomationElement* element);
	LocateResult LocateElement(IUIAutomationElement* element);
	IUIAutomationElement* GetSelectedElement() const;
	// the element's window, or its nearest ancestor's that has one
	HWND GetElementWindow(IUIAutomationElement* element) const;
	static bool HighlightElement(IUIAutomationElement* element);
	HRESULT DoAction(UINT id, IUIAutomationElement* element);

	void InitTree();
	// the node's children made the same as the element's (the nodes that stay are kept, expanded or not);
	// a node whose children have not been added only gets its "+" right; texts: the kept nodes' texts too
	void RefreshChildren(HTREEITEM hItem, bool texts = false);
	// the same (with the texts) for every node whose children were added
	void RefreshAll(HTREEITEM hItem);
	// UI Automation's structure and name changed events keep the tree up to date (while Live Updates is on)
	void StartEvents();
	void StopEvents();
	void ProcessEvents();
	void UpdateUI();
	void UpdateProperties(IUIAutomationElement* elem);
	void AddPatternProperties(IUIAutomationElement* elem);
	static std::wstring FormatValue(IUIAutomation* pUI, VARIANT const& value);

	LRESULT OnCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnDestroy(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnLocateElement(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnAutomationEvents(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnTimer(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnToggleLive(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnNodeExpanding(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/);
	LRESULT OnNodeExpanded(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/);
	LRESULT OnNodeSelected(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/);
	LRESULT OnNodeDeleted(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/);
	LRESULT OnTreeRightClick(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/);
	LRESULT OnRefresh(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnHighlight(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnElementAction(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);

	struct ItemData {
		PCWSTR Name;
		std::wstring Value;
	};

	// the selected element's patterns that have actions
	struct ElementPatterns {
		bool Invoke{}, Toggle{}, ExpandCollapse{}, SelectionItem{}, Value{}, ValueReadOnly{}, LegacyIAccessible{}, ScrollItem{};
	};
	// whether the action's command can be used with these patterns
	static bool IsActionEnabled(UINT id, ElementPatterns const& patterns);

	CCustomSplitterWindow m_Splitter;
	CTreeViewCtrl m_Tree;
	CListViewCtrl m_List;
	CComPtr<IUIAutomationTreeWalker> m_spUIWalker;
	CComPtr<IUIAutomation> m_spUI;
	CComPtr<IUIAutomationCacheRequest> m_spChildrenCache;
	std::vector<ItemData> m_Properties;
	ElementPatterns m_Patterns;
	// the nodes by their elements' runtime IDs (as text), and back; kept by TVN_DELETEITEM
	std::unordered_map<std::wstring, HTREEITEM> m_Nodes;
	std::unordered_map<HTREEITEM, std::wstring> m_Keys;
	CAutomationEventHandler* m_pEvents{ nullptr };
	bool m_Live{ true };
	bool m_EventsPending{ false };
};
