#pragma once

#include "Interfaces.h"
#include "WindowHelper.h"

template<typename T, typename TBase = CFrameWindowImpl<T, CWindow, CControlWinTraits>>
class CViewBase :
	public TBase,
	public CAutoUpdateUI<T>,
	public CIdleHandler {
public:
	CViewBase(IMainFrame* frame) : m_pFrame(frame), m_Dpi(WindowHelper::GetDpi()) {}

protected:
	BEGIN_MSG_MAP(CViewBase)
		MESSAGE_HANDLER(WM_DESTROY, OnDestroy)
		MESSAGE_HANDLER(TBVN_PAGEACTIVATED, OnTabActivated)
		MESSAGE_HANDLER(WM_DPICHANGED_AFTERPARENT, OnDpiChangedAfterParent)
		CHAIN_MSG_MAP(TBase)
	END_MSG_MAP()

	IMainFrame* GetFrame() {
		return m_pFrame;
	}

	void OnFinalMessage(HWND) override {
		delete this;
	}

	BOOL OnIdle() override {
		CAutoUpdateUI<T>::UIUpdateToolBar();
		return FALSE;
	}

	LRESULT OnTabActivated(UINT /*uMsg*/, WPARAM wParam, LPARAM, BOOL& bHandled) {
		auto pT = static_cast<T*>(this);
		pT->OnActivate(wParam ? true : false);
		return 0;
	}

	void OnActivate(bool activate) {}

	LRESULT OnDestroy(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& bHandled) {
		auto pT = static_cast<T*>(this);

		bHandled = FALSE;
		if (pT->m_hWndToolBar)
			_Module.GetMessageLoop()->RemoveIdleHandler(this);
		return 0;
	}

	// the main window moved to a monitor with another DPI (WindowHelper::SetDpi was called):
	// the toolbar's images and the rebar's bands (and their controls' font) are made for it
	LRESULT OnDpiChangedAfterParent(UINT, WPARAM, LPARAM, BOOL& bHandled) {
		auto pT = static_cast<T*>(this);
		bHandled = FALSE;
		auto dpi = WindowHelper::GetDpi();
		if (dpi == m_Dpi)
			return 0;

		if (pT->m_hWndToolBar) {
			CReBarCtrl rb(pT->m_hWndToolBar);
			int count = rb.GetBandCount();
			for (int i = 0; i < count; i++) {
				REBARBANDINFO rbbi{ sizeof(rbbi) };
				rbbi.fMask = RBBIM_CHILD | RBBIM_CHILDSIZE | RBBIM_SIZE | RBBIM_IDEALSIZE;
				rb.GetBandInfo(i, &rbbi);
				if (rbbi.hwndChild && rbbi.hwndChild == m_hViewToolBar) {
					CToolBarCtrl tb(m_hViewToolBar);
					tb.SetFont(WindowHelper::GetUIFont());
					SetToolBarImages();
					// the buttons made again (a toolbar keeps the sizes of its buttons' text), as they were
					std::vector<BYTE> states;
					for (auto& b : m_ToolBarButtons)
						states.push_back(b.id ? tb.GetState(b.id) : 0);
					while (tb.GetButtonCount() > 0)
						tb.DeleteButton(0);
					AddToolBarButtons(tb);
					for (size_t b = 0; b < m_ToolBarButtons.size(); b++)
						if (m_ToolBarButtons[b].id)
							tb.SetState(m_ToolBarButtons[b].id, states[b]);
					WindowHelper::FitToolBarBand(rb, tb);
					continue;
				}
				// a search box or such
				::SendMessage(rbbi.hwndChild, WM_SETFONT, (WPARAM)WindowHelper::GetUIFont(), TRUE);
				rbbi.fMask = RBBIM_CHILDSIZE | RBBIM_SIZE | RBBIM_IDEALSIZE;
				rbbi.cxMinChild = ::MulDiv(rbbi.cxMinChild, dpi, m_Dpi);
				rbbi.cyMinChild = ::MulDiv(rbbi.cyMinChild, dpi, m_Dpi);
				rbbi.cx = ::MulDiv(rbbi.cx, dpi, m_Dpi);
				rbbi.cxIdeal = ::MulDiv(rbbi.cxIdeal, dpi, m_Dpi);
				rb.SetBandInfo(i, &rbbi);
			}
		}
		m_Dpi = dpi;
		pT->UpdateLayout();
		return 0;
	}

	void SetToolBarImages() {
		CToolBarCtrl tb(m_hViewToolBar);
		int size = WindowHelper::DpiScale(24);
		CImageList images;
		images.Create(size, size, ILC_COLOR32 | ILC_COLOR | ILC_MASK, 4, 4);
		for (auto icon : m_ToolBarIcons)
			images.AddIcon(AtlLoadIconImage(icon, 0, size, size));
		CImageList old = tb.SetImageList(images);
		if (old)
			old.Destroy();
	}

	void AddToolBarButtons(CToolBarCtrl& tb) {
		int image = 0;
		for (auto& b : m_ToolBarButtons) {
			if (b.id == 0)
				tb.AddSeparator(0);
			else
				tb.AddButton(b.id, b.style | (b.text ? BTNS_SHOWTEXT : 0), TBSTATE_ENABLED, b.image == 0 ? I_IMAGENONE : image++, b.text, 0);
		}
	}

	HWND CreateAndInitToolBar(const ToolBarButtonInfo* buttons, int count) {
		auto pT = static_cast<T*>(this);

		CToolBarCtrl tb;
		auto hWndToolBar = tb.Create(pT->m_hWnd, pT->rcDefault, nullptr, ATL_SIMPLE_TOOLBAR_PANE_STYLE | TBSTYLE_LIST, 0, ATL_IDW_TOOLBAR);
		tb.SetExtendedStyle(TBSTYLE_EX_MIXEDBUTTONS);
		tb.SetFont(WindowHelper::GetUIFont());
		m_hViewToolBar = hWndToolBar;

		// the images' icons are kept, for making them at another DPI
		for (int i = 0; i < count; i++)
			if (buttons[i].id && buttons[i].image)
				m_ToolBarIcons.push_back(buttons[i].image);
		SetToolBarImages();

		m_ToolBarButtons.assign(buttons, buttons + count);
		AddToolBarButtons(tb);

		pT->CreateSimpleReBar(ATL_SIMPLE_REBAR_NOBORDER_STYLE);
		pT->AddSimpleReBarBand(tb);

		pT->UIAddToolBar(hWndToolBar);
		_Module.GetMessageLoop()->AddIdleHandler(this);

		return hWndToolBar;
	}

	IMainFrame* m_pFrame;
	HWND m_hViewToolBar{ nullptr };
	std::vector<int> m_ToolBarIcons;	// by image index
	std::vector<ToolBarButtonInfo> m_ToolBarButtons;	// (the texts are literals)
	UINT m_Dpi;		// the toolbar's and the bands' (WindowHelper::GetDpi() when they were made)
};
