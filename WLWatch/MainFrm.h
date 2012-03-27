// MainFrm.h : interface of the CMainFrame class
//
/////////////////////////////////////////////////////////////////////////////

#pragma once
#include "music_reader.h"
#define BOUNDS_CHECK
#undef min
#undef max
#include <dwt.h>
const int level=9;
class CMainFrame : 
	public CFrameWindowImpl<CMainFrame>, 
	public CUpdateUI<CMainFrame>,
	public CMessageFilter, public CIdleHandler
{
public:
	DECLARE_FRAME_WND_CLASS(NULL, IDR_MAINFRAME)

	CWLWatchView m_view;

	virtual BOOL PreTranslateMessage(MSG* pMsg)
	{
		if(CFrameWindowImpl<CMainFrame>::PreTranslateMessage(pMsg))
			return TRUE;

		return m_view.PreTranslateMessage(pMsg);
	}

	virtual BOOL OnIdle()
	{
		return FALSE;
	}

	BEGIN_UPDATE_UI_MAP(CMainFrame)
	END_UPDATE_UI_MAP()

	BEGIN_MSG_MAP_EX(CMainFrame)
		MESSAGE_HANDLER(WM_CREATE, OnCreate)
		MESSAGE_HANDLER(WM_DESTROY, OnDestroy)
		COMMAND_ID_HANDLER(ID_APP_EXIT, OnFileExit)
		COMMAND_ID_HANDLER(ID_FILE_OPEN, OnFileOpen)
		COMMAND_ID_HANDLER(ID_FILE_SAVE, OnFileSave)
		COMMAND_ID_HANDLER(ID_APP_ABOUT, OnAppAbout)
		CHAIN_MSG_MAP(CUpdateUI<CMainFrame>)
		CHAIN_MSG_MAP(CFrameWindowImpl<CMainFrame>)
	END_MSG_MAP()

// Handler prototypes (uncomment arguments if needed):
//	LRESULT MessageHandler(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
//	LRESULT CommandHandler(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
//	LRESULT NotifyHandler(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/)

	LRESULT OnCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
	{

		m_hWndClient = m_view.Create(m_hWnd, rcDefault, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN, WS_EX_CLIENTEDGE);

		// register object for message filtering and idle updates
		CMessageLoop* pLoop = _Module.GetMessageLoop();
		ATLASSERT(pLoop != NULL);
		pLoop->AddMessageFilter(this);
		pLoop->AddIdleHandler(this);

		return 0;
	}

	LRESULT OnDestroy(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled)
	{
		// unregister message filtering and idle updates
		CMessageLoop* pLoop = _Module.GetMessageLoop();
		ATLASSERT(pLoop != NULL);
		pLoop->RemoveMessageFilter(this);
		pLoop->RemoveIdleHandler(this);

		bHandled = FALSE;
		return 1;
	}

	LRESULT OnFileExit(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	{
		PostMessage(WM_CLOSE);
		return 0;
	}

	CAtlString openFileName;
	LRESULT OnFileOpen(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	{
		CFileDialog openfile(TRUE);
		if(IDOK!=openfile.DoModal())
			return S_FALSE;

		openFileName=openfile.m_szFileName;
		openFileName=openFileName.Right(openFileName.GetLength()-openFileName.ReverseFind('\\')-1);
		openFileName=openFileName.Left(openFileName.Find('.'));
		std::vector<std::vector<double>> dataline= ReadMusicSrcData(openfile.m_szFileName);
		
		std::vector<double> fulldata;
		for(auto i=dataline.begin();i!=dataline.end();i++)
		{
			fulldata.insert(fulldata.end(),i->begin(),i->end());
		}
		dataline.clear();
		
		BuildData(fulldata);
		return 0;
	}
	LRESULT OnFileSave(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	{
		SaveData();
		return 0;
	}
	std::vector<splab::Vector<double> > levels;
	std::vector<std::vector<int> > levelcheckpoint;
	void BuildData(std::vector<double> &fulldata)
	{
		splab::Vector<double> srcline(fulldata.size(),&fulldata[0]);
		splab::DWT<double> wt("db4");
		splab::Vector<double> CL= wt.dwt(srcline,level);
		levels.clear();
		for(int i=1;i<=level;i++)
		{
			splab::Vector<double> detialline=wt.getDetial(CL,i);
			for(int j=0;j<detialline.size();j++)
			{
				detialline[j]=abs(detialline[j]);
			}
			levels.push_back(detialline);
		}

		levelcheckpoint.clear();
		const int CheckRange=10;
		for(auto i=levels.begin();i!=levels.end();i++)
		{
			double lmax=0;
			for(int pos=0;pos<i->size();pos++)
			{
				lmax=max((*i)[pos],lmax);
			}
			for(int pos=0;pos<i->size();pos++)
			{
				(*i)[pos]/=lmax;
			}

			std::vector<int> poslist;
			for(int pos=CheckRange;pos<i->size()-CheckRange;pos++)
			{
				if((*i)[pos]>0.5)
				{
					double tmax=0;
					for(int ckpos=pos-CheckRange;ckpos<=pos+CheckRange;ckpos++)
						tmax=max((*i)[ckpos],tmax);
					if(tmax==(*i)[pos])
						poslist.push_back(pos);
				}
			}
			levelcheckpoint.push_back(poslist);
		}
	}
	void SaveData()
	{
		if(openFileName.IsEmpty())
			return;
		CSqlite db;
		db.Open(L"D:\\wl_info.data.db");

		CSqliteStmt insertFileName=db.Prepare(L"insert into songlist(name) values(?1)");
		CSqliteStmt lastFileId=db.Prepare(L"select last_insert_rowid() from songlist");
		CSqliteStmt insertLevel9Anchor=db.Prepare(L"insert into level9_anchor(song_id,pos,after_check) values(?1,?2,?3)");

		/*int song_id=0;
		insertFileName.Bind(1,openFileName);
		if(SQLITE_DONE==insertFileName.Step())
		{
			if(SQLITE_ROW==lastFileId.Step())
			{
				song_id=lastFileId.GetInt(0);
			}
			lastFileId.Reset();
		}
		insertFileName.Reset();

		if(song_id>0)*/
		{
			auto &line9=levelcheckpoint[8];
			for(auto i=line9.begin();i!=line9.end();i++)
			{
				std::vector<int> checks;
				for(auto j=i+1;j<line9.end();j++)
				{
					int span=(*j)-(*i);
					if(span>500)
						break;
					checks.push_back(span);
				}
			}
		}
	}

	LRESULT OnAppAbout(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	{
		CAboutDlg dlg;
		dlg.DoModal();
		return 0;
	}
};
