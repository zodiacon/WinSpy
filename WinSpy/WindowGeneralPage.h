#pragma once

#include "DialogHelper.h"
#include "WindowHelper.h"

class CWindowGeneralPage :
	public CDialogHelper<CWindowGeneralPage>,
	public CPropertyPageImpl<CWindowGeneralPage>,
	public CDynamicDialogLayout<CWindowGeneralPage> {
public:
	enum { IDD = IDD_WINPROP };

	const UINT WM_UPDATE = WM_USER + 300;

	CWindowGeneralPage(HWND hWnd) : m_Win(hWnd) {
		m_psp.dwFlags |= PSP_USEICONID;
		m_psp.pszIcon = MAKEINTRESOURCE(IDI_WINPROP);
	}

	void UpdateData();

	// refreshed every second while showing, if anything changed
	int OnSetActive();
	BOOL OnKillActive();

protected:
	BEGIN_MSG_MAP(CWindowGeneralPage)
		MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
		MESSAGE_HANDLER(WM_UPDATE, OnUpdate)
		MESSAGE_HANDLER(WM_TIMER, OnTimer)
		CHAIN_MSG_MAP(CPropertyPageImpl<CWindowGeneralPage>)
		CHAIN_MSG_MAP(CDynamicDialogLayout<CWindowGeneralPage>)
	END_MSG_MAP()

private:
	void FillStyleList(DWORD style, std::pair<StyleItem const*, int> styles, UINT id, PCWSTR prefix, PCWSTR defaultStyle, bool clear = true);
	void FillSpecificControlStyles();
	// what the page shows, in short: it is updated when this changes
	CString GetSnapshot() const;

//	LRESULT MessageHandler(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
//	LRESULT CommandHandler(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
//	LRESULT NotifyHandler(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/)

	LRESULT OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnUpdate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnTimer(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);

	CWindow m_Win;
	CString m_Snapshot;
};

//
// the window's styles (with those of its kind of control) and extended styles as check boxes, to change them
//
class CWindowStylesPage :
	public CDialogHelper<CWindowStylesPage>,
	public CPropertyPageImpl<CWindowStylesPage>,
	public CDynamicDialogLayout<CWindowStylesPage> {
public:
	enum { IDD = IDD_PROPSTYLES };

	const UINT WM_UPDATE = WM_USER + 300;

	CWindowStylesPage(HWND hWnd) : m_Win(hWnd) {
		m_psp.dwFlags |= PSP_USEICONID;
		m_psp.pszIcon = MAKEINTRESOURCE(IDI_WINPROP);
	}

	int OnSetActive();
	BOOL OnKillActive();

protected:
	BEGIN_MSG_MAP(CWindowStylesPage)
		MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
		MESSAGE_HANDLER(WM_UPDATE, OnUpdate)
		MESSAGE_HANDLER(WM_TIMER, OnTimer)
		MESSAGE_HANDLER(WM_SIZE, OnSize)
		NOTIFY_HANDLER(IDC_STYLES, LVN_ITEMCHANGED, OnItemChanged)
		NOTIFY_HANDLER(IDC_STYLESEX, LVN_ITEMCHANGED, OnItemChanged)
		COMMAND_ID_HANDLER(IDC_APPLY, OnApplyStyles)
		COMMAND_ID_HANDLER(IDC_RESET, OnRevertStyles)
		CHAIN_MSG_MAP(CPropertyPageImpl<CWindowStylesPage>)
		CHAIN_MSG_MAP(CDynamicDialogLayout<CWindowStylesPage>)
	END_MSG_MAP()

private:
	struct StyleList {
		CListViewCtrl List;
		std::vector<StyleItem> Items;
		DWORD Original{ 0 };	// the window's
		DWORD Value{ 0 };		// with the user's changes
		UINT EditId;
	};

	// reads the window's styles, dropping changes not applied
	void Load();
	void FillList(StyleList& styles, std::vector<std::pair<StyleItem const*, int>> const& arrays);
	void ShowStyle(StyleList& styles);
	bool IsChanged() const;

	LRESULT OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnUpdate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnTimer(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnItemChanged(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/);
	LRESULT OnSize(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnApplyStyles(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnRevertStyles(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);

	CWindow m_Win;
	StyleList m_Styles, m_ExStyles;
	bool m_Updating{ false };	// the check boxes are being set by the page, not the user
};
