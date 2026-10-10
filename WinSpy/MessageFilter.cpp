#include "pch.h"
#include "MessageFilter.h"
#include "WindowHelper.h"

namespace {
	const UINT WM_SYSTIMER = 0x118;

	bool InRange(UINT msg, UINT first, UINT last) {
		return msg >= first && msg <= last;
	}

	// message name (upper case) to number, for the names WindowHelper knows
	std::unordered_map<std::wstring, UINT> const& MessageNames() {
		static std::unordered_map<std::wstring, UINT> names;
		if (names.empty()) {
			auto add = [](UINT msg) {
				auto name = WindowHelper::WindowMessageToString(msg);
				if (!name.IsEmpty() && name.Find(L'+') < 0)
					names.insert({ std::wstring(name.MakeUpper()), msg });
			};
			for (UINT msg = 0; msg <= WM_USER; msg++)
				add(msg);
			add(WM_APP);
		}
		return names;
	}
}

MessageCategory MessageFilter::Categorize(UINT msg) {
	switch (msg) {
		case WM_NCCREATE: case WM_NCDESTROY: case WM_NCCALCSIZE: case WM_NCHITTEST: case WM_NCPAINT: case WM_NCACTIVATE:
		case WM_NCMOUSEHOVER: case WM_NCMOUSELEAVE: case WM_NCPOINTERUPDATE: case WM_NCPOINTERDOWN: case WM_NCPOINTERUP:
			return MessageCategory::NonClient;

		case WM_MOUSEHOVER: case WM_MOUSELEAVE: case WM_CAPTURECHANGED: case WM_SETCURSOR: case WM_MOUSEACTIVATE:
			return MessageCategory::Mouse;

		case WM_HOTKEY: case WM_INPUT: case WM_INPUTLANGCHANGE: case WM_INPUTLANGCHANGEREQUEST:
			return MessageCategory::Keyboard;

		case WM_TIMER: case WM_SYSTIMER:
			return MessageCategory::Timer;

		case WM_PAINT: case WM_ERASEBKGND: case WM_SYNCPAINT: case WM_PRINT: case WM_PRINTCLIENT: case WM_DRAWITEM:
		case WM_MEASUREITEM: case WM_SETREDRAW: case WM_ICONERASEBKGND: case WM_PAINTICON:
			return MessageCategory::Paint;

		case WM_CREATE: case WM_DESTROY: case WM_MOVE: case WM_SIZE: case WM_ACTIVATE: case WM_SETFOCUS: case WM_KILLFOCUS:
		case WM_ENABLE: case WM_SHOWWINDOW: case WM_CLOSE: case WM_WINDOWPOSCHANGING: case WM_WINDOWPOSCHANGED:
		case WM_STYLECHANGING: case WM_STYLECHANGED: case WM_GETMINMAXINFO: case WM_SIZING: case WM_MOVING:
		case WM_ENTERSIZEMOVE: case WM_EXITSIZEMOVE: case WM_ACTIVATEAPP: case WM_SETTEXT: case WM_GETTEXT:
		case WM_GETTEXTLENGTH: case WM_SETICON: case WM_GETICON: case WM_SETFONT: case WM_GETFONT: case WM_DPICHANGED:
		case WM_PARENTNOTIFY: case WM_CHILDACTIVATE: case WM_QUERYOPEN:
			return MessageCategory::Window;
	}

	if (InRange(msg, WM_NCMOUSEMOVE, WM_NCXBUTTONDBLCLK))
		return MessageCategory::NonClient;
	if (InRange(msg, WM_MOUSEFIRST, WM_MOUSELAST) || InRange(msg, WM_POINTERDEVICECHANGE, WM_POINTERHWHEEL))
		return MessageCategory::Mouse;
	if (InRange(msg, WM_KEYFIRST, WM_KEYLAST) || InRange(msg, WM_IME_STARTCOMPOSITION, WM_IME_KEYLAST) || InRange(msg, WM_IME_SETCONTEXT, WM_IME_KEYUP))
		return MessageCategory::Keyboard;
	if (InRange(msg, WM_CTLCOLORMSGBOX, WM_CTLCOLORSTATIC))
		return MessageCategory::Paint;

	return MessageCategory::Other;
}

bool MessageFilter::IsVisible(MessageKind kind, UINT msg) const {
	switch (kind) {
		case MessageKind::Posted: if (!Posted) return false; break;
		case MessageKind::Sent: if (!Sent) return false; break;
		case MessageKind::Returned: if (!Returned) return false; break;
	}
	if (!Include.empty())
		return Include.contains(msg);
	if (Exclude.contains(msg))
		return false;
	return (Categories & Categorize(msg)) != MessageCategory::None;
}

std::unordered_set<UINT> MessageFilter::ParseList(CString const& text, CString* errors) {
	std::unordered_set<UINT> list;
	int start = 0;
	for (;;) {
		auto token = text.Tokenize(L", ;\t\r\n", start);
		if (start < 0)
			break;

		PWSTR end;
		ULONG value;
		if (token.Left(2).CompareNoCase(L"0x") == 0)
			value = wcstoul(token.Mid(2), &end, 16);
		else
			value = wcstoul(token, &end, 10);
		if (*end == 0 && !token.IsEmpty() && value <= 0xFFFF) {
			list.insert(value);
			continue;
		}

		auto& names = MessageNames();
		if (auto it = names.find(std::wstring(token.MakeUpper())); it != names.end())
			list.insert(it->second);
		else if (errors)
			*errors += token + L" ";
	}
	return list;
}

CString MessageFilter::FormatList(std::unordered_set<UINT> const& list) {
	std::vector<UINT> sorted(list.begin(), list.end());
	std::sort(sorted.begin(), sorted.end());

	CString text;
	for (auto msg : sorted) {
		auto name = WindowHelper::WindowMessageToString(msg);
		if (name.IsEmpty() || name.Find(L'+') >= 0)
			name.Format(L"0x%X", msg);
		text += (text.IsEmpty() ? L"" : L", ") + name;
	}
	return text;
}

std::unordered_set<UINT> const& MessageFilter::NoisyMessages() {
	static const std::unordered_set<UINT> noisy = {
		WM_MOUSEMOVE, WM_NCMOUSEMOVE, WM_NCHITTEST, WM_SETCURSOR, WM_TIMER, WM_SYSTIMER, WM_MOUSEHOVER, WM_POINTERUPDATE,
	};
	return noisy;
}

CString MessageFilter::ToString() const {
	auto list = [](auto const& messages) {
		CString text, number;
		for (auto msg : messages) {
			number.Format(L"%u ", msg);
			text += number;
		}
		return text;
	};
	CString text;
	text.Format(L"%d%d%d|%u|%s|%s", Posted, Sent, Returned, (DWORD)Categories, (PCWSTR)list(Exclude), (PCWSTR)list(Include));
	return text;
}

void MessageFilter::FromString(CString const& text) {
	int start = 0;
	auto kinds = text.Tokenize(L"|", start);
	if (kinds.GetLength() != 3)
		return;

	Posted = kinds[0] == L'1';
	Sent = kinds[1] == L'1';
	Returned = kinds[2] == L'1';
	// Tokenize skips empty fields, so the lists are split by hand
	auto rest = text.Mid(text.Find(L'|') + 1);
	auto parts = [&](int index) {
		int pos = 0;
		for (int i = 0; i < index; i++) {
			pos = rest.Find(L'|', pos);
			if (pos < 0)
				return CString();
			pos++;
		}
		auto end = rest.Find(L'|', pos);
		return end < 0 ? rest.Mid(pos) : rest.Mid(pos, end - pos);
	};
	Categories = (MessageCategory)wcstoul(parts(0), nullptr, 10) & MessageCategory::All;
	Exclude = ParseList(parts(1));
	Include = ParseList(parts(2));
}
