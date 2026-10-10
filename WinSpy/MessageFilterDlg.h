#pragma once

#include "resource.h"
#include "MessageFilter.h"

class CMessageFilterDlg : public CDialogImpl<CMessageFilterDlg> {
public:
	enum { IDD = IDD_MESSAGEFILTER };

	explicit CMessageFilterDlg(MessageFilter const& filter) : m_Filter(filter) {}

	MessageFilter const& GetFilter() const {
		return m_Filter;
	}

	BEGIN_MSG_MAP(CMessageFilterDlg)
		MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
		COMMAND_ID_HANDLER(IDC_PRESET, OnPreset)
		COMMAND_ID_HANDLER(IDC_RESET, OnReset)
		COMMAND_ID_HANDLER(IDOK, OnOK)
		COMMAND_ID_HANDLER(IDCANCEL, OnCancel)
	END_MSG_MAP()

private:
	void SetControls(MessageFilter const& filter);

	LRESULT OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnPreset(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnReset(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnOK(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnCancel(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);

	MessageFilter m_Filter;
};
