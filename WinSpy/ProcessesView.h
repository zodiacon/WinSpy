#pragma once

#include "ViewBase.h"
#include "WindowsListView.h"

class CProcessesView : 
	public CViewBase<CProcessesView> {
public:
	CProcessesView(IMainFrame* frame) : CViewBase(frame) {}


private:
	CWindowsListView m_WindowsView;
};
