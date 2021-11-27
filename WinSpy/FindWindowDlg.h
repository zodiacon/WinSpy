#pragma once

#include "Interfaces.h"
#include "DialogHelper.h"

class CFindWindowDlg : 
	public CDialogImpl<CFindWindowDlg>,
	public CDynamicDialogLayout<CFindWindowDlg>,
	public CDialogHelper<CFindWindowDlg> {
public:
	enum { IDD = IDD_FINDWINDOW };

	CFindWindowDlg(IMainFrame* frame) : m_pFrame(frame), m_WinDrag(this, 1) {}

	HWND GetSelectedHwnd() const;

protected:
	BEGIN_MSG_MAP(CFindWindowDlg)
		MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
		COMMAND_ID_HANDLER(IDC_SEARCH, OnSearch)
		COMMAND_ID_HANDLER(IDOK, OnCloseCmd)
		COMMAND_ID_HANDLER(IDCANCEL, OnCloseCmd)
		CHAIN_MSG_MAP(CDynamicDialogLayout<CFindWindowDlg>)
	ALT_MSG_MAP(1)
		MESSAGE_HANDLER(WM_MOUSEMOVE, OnMouseMove)
		MESSAGE_HANDLER(WM_LBUTTONUP, OnMouseUp)
		MESSAGE_HANDLER(WM_LBUTTONDOWN, OnMouseDown)
	END_MSG_MAP()

	// Handler prototypes (uncomment arguments if needed):
//	LRESULT MessageHandler(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
//	LRESULT CommandHandler(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
//	LRESULT NotifyHandler(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/)

private:
	void ClearWindowDetails();
	void SelectWindow(HWND hWnd);

	LRESULT OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnCloseCmd(WORD /*wNotifyCode*/, WORD id, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnSearch(WORD /*wNotifyCode*/, WORD id, HWND /*hWndCtl*/, BOOL& /*bHandled*/);

	LRESULT OnMouseDown(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnMouseUp(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnMouseMove(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);

	CContainedWindowT<CStatic> m_WinDrag;
	CIcon m_DragCursor;
	CWindow m_hCursorWnd;
	IMainFrame* m_pFrame;
	HWND m_SelectedHwnd{ nullptr };
	bool m_Capture{ false };
};

