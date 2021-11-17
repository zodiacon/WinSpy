#pragma once

#include "Interfaces.h"

template<typename T>
struct CFrameWindowHelper : CIdleHandler {
	HWND CreateAndInitToolBar(const ToolBarButtonInfo* buttons, int count) {
		auto pT = static_cast<T*>(this);
		CToolBarCtrl tb;
		auto hWndToolBar = tb.Create(pT->m_hWnd, CWindow::rcDefault, nullptr, ATL_SIMPLE_TOOLBAR_PANE_STYLE | TBSTYLE_LIST, 0, ATL_IDW_TOOLBAR);
		tb.SetExtendedStyle(TBSTYLE_EX_MIXEDBUTTONS);

		CImageList tbImages;
		tbImages.Create(24, 24, ILC_COLOR32 | ILC_COLOR | ILC_MASK, 4, 4);
		tb.SetImageList(tbImages);

		for (int i = 0; i < count; i++) {
			auto& b = buttons[i];
			if (b.id == 0)
				tb.AddSeparator(0);
			else {
				int image = b.image == 0 ? I_IMAGENONE : tbImages.AddIcon(AtlLoadIconImage(b.image, 0, 24, 24));
				tb.AddButton(b.id, b.style | (b.text ? BTNS_SHOWTEXT : 0), TBSTATE_ENABLED, image, b.text, 0);
			}
		}

		pT->CreateSimpleReBar(ATL_SIMPLE_REBAR_NOBORDER_STYLE);
		pT->AddSimpleReBarBand(tb);

		pT->UIAddToolBar(hWndToolBar);
		_Module.GetMessageLoop()->AddIdleHandler(this);

		return hWndToolBar;
	}

	BOOL OnIdle() override {
		auto pT = static_cast<T*>(this);
		pT->UIUpdateToolBar();
		return FALSE;
	}
};
