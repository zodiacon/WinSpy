#include "pch.h"
#include "MessageFilterDlg.h"

namespace {
	const struct {
		UINT id;
		MessageCategory category;
	} Categories[] = {
		{ IDC_CAT_MOUSE, MessageCategory::Mouse },
		{ IDC_CAT_KEYBOARD, MessageCategory::Keyboard },
		{ IDC_CAT_NONCLIENT, MessageCategory::NonClient },
		{ IDC_CAT_TIMER, MessageCategory::Timer },
		{ IDC_CAT_PAINT, MessageCategory::Paint },
		{ IDC_CAT_WINDOW, MessageCategory::Window },
		{ IDC_CAT_OTHER, MessageCategory::Other },
	};
}

void CMessageFilterDlg::SetControls(MessageFilter const& filter) {
	CheckDlgButton(IDC_POSTED, filter.Posted);
	CheckDlgButton(IDC_SENT, filter.Sent);
	CheckDlgButton(IDC_RETURNED, filter.Returned);
	for (auto& c : Categories)
		CheckDlgButton(c.id, (filter.Categories & c.category) == c.category);
	SetDlgItemText(IDC_EXCLUDE, MessageFilter::FormatList(filter.Exclude));
	SetDlgItemText(IDC_INCLUDE, MessageFilter::FormatList(filter.Include));
}

LRESULT CMessageFilterDlg::OnInitDialog(UINT, WPARAM, LPARAM, BOOL&) {
	SetControls(m_Filter);
	CenterWindow(GetParent());
	return TRUE;
}

LRESULT CMessageFilterDlg::OnPreset(WORD, WORD, HWND, BOOL&) {
	// adds the noisy messages to what is already excluded
	CString text;
	GetDlgItemText(IDC_EXCLUDE, text);
	auto exclude = MessageFilter::ParseList(text);
	for (auto msg : MessageFilter::NoisyMessages())
		exclude.insert(msg);
	SetDlgItemText(IDC_EXCLUDE, MessageFilter::FormatList(exclude));
	return 0;
}

LRESULT CMessageFilterDlg::OnReset(WORD, WORD, HWND, BOOL&) {
	SetControls(MessageFilter());
	return 0;
}

LRESULT CMessageFilterDlg::OnOK(WORD, WORD, HWND, BOOL&) {
	MessageFilter filter;
	filter.Posted = IsDlgButtonChecked(IDC_POSTED) == BST_CHECKED;
	filter.Sent = IsDlgButtonChecked(IDC_SENT) == BST_CHECKED;
	filter.Returned = IsDlgButtonChecked(IDC_RETURNED) == BST_CHECKED;
	filter.Categories = MessageCategory::None;
	for (auto& c : Categories)
		if (IsDlgButtonChecked(c.id) == BST_CHECKED)
			filter.Categories |= c.category;

	CString text, errors;
	GetDlgItemText(IDC_EXCLUDE, text);
	filter.Exclude = MessageFilter::ParseList(text, &errors);
	GetDlgItemText(IDC_INCLUDE, text);
	filter.Include = MessageFilter::ParseList(text, &errors);
	if (!errors.IsEmpty()) {
		AtlMessageBox(m_hWnd, (PCWSTR)(L"Unknown messages: " + errors), IDS_TITLE, MB_ICONWARNING);
		return 0;
	}

	m_Filter = std::move(filter);
	EndDialog(IDOK);
	return 0;
}

LRESULT CMessageFilterDlg::OnCancel(WORD, WORD, HWND, BOOL&) {
	EndDialog(IDCANCEL);
	return 0;
}
