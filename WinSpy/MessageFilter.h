#pragma once

#include <unordered_set>

enum class MessageCategory : DWORD {
	None = 0,
	Mouse = 1,
	Keyboard = 2,
	NonClient = 4,
	Timer = 8,
	Paint = 0x10,
	Window = 0x20,
	Other = 0x40,
	All = 0x7F
};
DEFINE_ENUM_FLAG_OPERATORS(MessageCategory);

enum class MessageKind {
	Posted,		// WH_GETMESSAGE
	Sent,		// WH_CALLWNDPROC
	Returned,	// WH_CALLWNDPROCRET
};

//
// which captured messages the Messages view shows
//
struct MessageFilter {
	bool Posted{ true }, Sent{ true }, Returned{ true };
	MessageCategory Categories{ MessageCategory::All };
	std::unordered_set<UINT> Exclude;
	std::unordered_set<UINT> Include;		// empty: all

	bool IsVisible(MessageKind kind, UINT msg) const;

	static MessageCategory Categorize(UINT msg);

	// message names or numbers ("WM_TIMER, 0x113 275"), separated by commas or spaces;
	// what isn't understood is put in 'errors' (space separated)
	static std::unordered_set<UINT> ParseList(CString const& text, CString* errors = nullptr);
	static CString FormatList(std::unordered_set<UINT> const& list);

	// the noisy ones: mouse moves, hit testing, cursor setting and timers
	static std::unordered_set<UINT> const& NoisyMessages();

	// for keeping in the settings
	CString ToString() const;
	void FromString(CString const& text);
};
