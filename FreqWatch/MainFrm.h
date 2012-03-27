// MainFrm.h : interface of the CMainFrame class
//
/////////////////////////////////////////////////////////////////////////////

#pragma once

#include "DIBBitmap.h"
#include "music_reader.h"
#include "UploadFreqData.h"
#include "SearchBySite.h"
#include "..\WavSink\Fourier.h"
class CMainFrame : 
	public CFrameWindowImpl<CMainFrame>, 
	public CUpdateUI<CMainFrame>,
	public CMessageFilter, public CIdleHandler
{
public:
	DECLARE_FRAME_WND_CLASS(NULL, IDR_MAINFRAME)

	CFreqWatchView m_view;
	CTrackBarCtrl m_trackBar;

	virtual BOOL PreTranslateMessage(MSG* pMsg)
	{
		if(CFrameWindowImpl<CMainFrame>::PreTranslateMessage(pMsg))
			return TRUE;

		return m_view.PreTranslateMessage(pMsg);
	}

	virtual BOOL OnIdle()
	{
		UIUpdateToolBar();
		return FALSE;
	}

	BEGIN_UPDATE_UI_MAP(CMainFrame)
		UPDATE_ELEMENT(ID_VIEW_TOOLBAR, UPDUI_MENUPOPUP)
	END_UPDATE_UI_MAP()

	BEGIN_MSG_MAP_EX(CMainFrame)
		MESSAGE_HANDLER(WM_CREATE, OnCreate)
		MESSAGE_HANDLER(WM_DESTROY, OnDestroy)
		MSG_WM_SIZE(OnSize)
		MSG_WM_HSCROLL(OnHScroll)
		MESSAGE_HANDLER(WM_CLIENTMOUSEMOVE,OnClientMouseMove)
		COMMAND_ID_HANDLER(ID_APP_EXIT, OnFileExit)
		COMMAND_ID_HANDLER(ID_FILE_NEW, OnFileNew)
		COMMAND_ID_HANDLER(ID_FILE_SAVE,OnFileSave)
		COMMAND_ID_HANDLER(ID_FILE_SAVE_AS,OnUploadData)
		COMMAND_ID_HANDLER(ID_FILE_OPEN,OnFileOpen)
		COMMAND_ID_HANDLER(ID_FILE_SEARCH_VAR_SITE,OnSearchVarSite)
		COMMAND_ID_HANDLER(ID_VIEW_TOOLBAR, OnViewToolBar)
		COMMAND_ID_HANDLER(ID_FILE_RECORD,OnFileRecord)
		COMMAND_ID_HANDLER(ID_FILE_RUN_FOLDER,OnRunFolder)
		CHAIN_MSG_MAP(CUpdateUI<CMainFrame>)
		CHAIN_MSG_MAP(CFrameWindowImpl<CMainFrame>)
	END_MSG_MAP()

// Handler prototypes (uncomment arguments if needed):
//	LRESULT MessageHandler(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
//	LRESULT CommandHandler(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
//	LRESULT NotifyHandler(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/)

	LRESULT OnCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
	{
		//CreateSimpleToolBar();

		m_view.Create(m_hWnd, rcDefault, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN);
		m_view.image=&memimage;
		m_view.freqinfos=&freqinfos;
		m_trackBar.Create(m_hWnd,rcDefault,NULL,WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN,0,5);
		m_trackBar.SetRange(0,100);

		//UIAddToolBar(m_hWndToolBar);
		//UISetCheck(ID_VIEW_TOOLBAR, 1);

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
	std::vector<std::vector<double>> dataline;
	CDIBBitmap memimage;
	
	double maxStrong;
	CAtlString openFileName;
	LRESULT OnFileNew(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	{
		CFileDialog openfile(TRUE);
		if(IDOK!=openfile.DoModal())
			return S_FALSE;

		openFileName=openfile.m_szFileName;
		openFileName=openFileName.Right(openFileName.GetLength()-openFileName.ReverseFind('\\')-1);
		openFileName=openFileName.Left(openFileName.Find('.'));
		dataline= ReadMusicFrequencyData(openfile.m_szFileName);
		
		m_trackBar.SetRangeMax(100);
		m_trackBar.SetPos(50);
		BuildData();
		BuildImage();
		return 0;
	}
	LRESULT OnFileSave(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	{
		CAboutDlg dlg;
		dlg.filename=openFileName;
		if(dlg.DoModal()!=IDOK)
			return S_OK;
		dlg.filename.Trim(L"\t\n ");
		if(dlg.filename.IsEmpty())
		{
			MessageBox(L"歌曲名是空的,请检查.");
			return S_OK;
		}
		
		SaveMusicInfoToDb(dlg.filename);
		return S_OK;
	}
	void SaveMusicInfoToDb(CAtlString title)
	{
		CSqlite db;
		db.Open(L"D:\\freq_info.data.db");
		CSqliteStmt insertFileName=db.Prepare(L"insert into songlist(name) values(?1)");
		CSqliteStmt lastFileId=db.Prepare(L"select last_insert_rowid() from songlist");
		CSqliteStmt insertACPair=db.Prepare(L"insert into Anchor_Check_list(song_id,anchor_time,anchor_freq,check_freq,time_offset) values"
			L"(?1,?2,?3,?4,?5)");

		int song_id=0;
		insertFileName.Bind(1,title);
		if(SQLITE_DONE==insertFileName.Step())
		{
			if(SQLITE_ROW==lastFileId.Step())
			{
				song_id=lastFileId.GetInt(0);
			}
			lastFileId.Reset();
		}
		insertFileName.Reset();

		if(song_id>0)
		{
			db.Execute(L"begin transaction");
			std::vector<FreqInfo> freqinfos_use;
			for(auto i=freqinfos.begin();i<freqinfos.end();i++)
			{
				if(i->freq>40 && i->freq<600)
				{
					freqinfos_use.push_back(*i);
				}
			}
			int ptcount=0;
			for(auto i=freqinfos_use.begin();i<freqinfos_use.end();i++)
			{
				std::vector<FreqInfo> anchor_sub;
				for(auto j=i+1;j<freqinfos_use.end();j++)
				{
					if(j->time>i->time+45)
						break;
					if(j->freq>i->freq-30 && j->freq<i->freq+30 &&
						j->time>i->time+5)
					{
						anchor_sub.push_back(*j);
					}
				}
				if(anchor_sub.size()>3)
				{
					if(anchor_sub.size()>3)
					{
						for(auto j=anchor_sub.begin();j!=anchor_sub.end();j++)
						{
							insertACPair.Bind(1,song_id);
							insertACPair.Bind(2,i->time);
							insertACPair.Bind(3,i->freq);
							insertACPair.Bind(4,j->freq);
							insertACPair.Bind(5,j->time-i->time);
							insertACPair.Step();
							insertACPair.Reset();
						}
					}
				}
			}
			db.Execute(L"commit transaction");
		}
	}
	LRESULT OnFileOpen(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	{
		struct FreqPointPicked
		{
			int song_id;
			int offset_time;
		};

		CSqlite db;
		db.Open(L"D:\\freq_info.data.db",SQLITE_OPEN_READONLY);
		CSqliteStmt testAnchor=db.Prepare(L"select song_id,anchor_time from Anchor_Check_list where anchor_freq=?1 and check_freq=?2 and time_offset=?3");

		std::vector<FreqInfo> freqinfos_use;
		std::map<FreqInfo*,std::map<int,int> > check_use_count;
		for(auto i=freqinfos.begin();i<freqinfos.end();i++)
		{
			if(i->freq>40 && i->freq<350)
			{
				freqinfos_use.push_back(*i);
			}
		}
		vector<FreqPointPicked> checkres;
		int maybecount=0;
		for(auto i=freqinfos_use.begin();i<freqinfos_use.end();i++)
		{
			/*auto used=check_use_count.find(&(*i));
			if(used!=check_use_count.end())
			{
				bool notcheck=false;
				for(auto j=used->second.begin();j!=used->second.end();j++)
				{
					if(j->second>=2)
					{
						notcheck=true;
						break;
					}
				}
				if(notcheck)
					continue;
			}*/

			for(auto j=i+1;j!=freqinfos_use.end();j++)
			{
				maybecount++;
				int time_offset=j->time-i->time;
				if(time_offset>45)
					break;
				if(j->freq>i->freq-30 && j->freq<i->freq+30 &&
						j->time>i->time+5)
				{
					testAnchor.Bind(1,i->freq);
					testAnchor.Bind(2,j->freq);
					testAnchor.Bind(3,time_offset);

					while(SQLITE_ROW==testAnchor.Step())
					{
						int songid=testAnchor.GetInt(0);
						int anchor_time=testAnchor.GetInt(1);

						FreqPointPicked pickedpoint;
						pickedpoint.song_id=songid;
						pickedpoint.offset_time=anchor_time-i->time;
						checkres.push_back(pickedpoint);

						check_use_count[&(*j)][songid]++;
					}
					testAnchor.Reset();
				}
			}
		}

		CAtlString resinfo;
		resinfo.AppendFormat(_T("all Anchor checked:%u\n"),maybecount);
		struct MatchInfo
		{
			int count;
			int starttimeMaxCount;
			std::map<int,int> timematch;
			MatchInfo()
			{
				count=0;
				starttimeMaxCount=0;
			}
		};
		std::map<int,MatchInfo> matchcountor;
		for(auto i=checkres.begin();i!=checkres.end();i++)
		{
			auto& countor=matchcountor[i->song_id];
			countor.count++;
			countor.timematch[i->offset_time]++;
		}

		
		for(auto countor=matchcountor.begin();countor!=matchcountor.end();countor++)
		{
			int maxpos=0;
			int maxcount=0;
			for(auto j=countor->second.timematch.begin();
				j!=countor->second.timematch.end();
				j++)
			{
				if(j->second>maxcount)
				{
					maxcount=j->second;
					maxpos=j->first;
				}
			}
			maxcount=0;
			for(int fr=maxpos-3;fr<=maxpos+3;fr++)
			{
				maxcount+=countor->second.timematch[fr];
			}
			countor->second.starttimeMaxCount=maxcount;
		}

		for(auto i=matchcountor.begin();i!=matchcountor.end();++i)
		{
			resinfo.AppendFormat(_T("found song:%d (%d time,startMatch %d)\n"),i->first,i->second.count,i->second.starttimeMaxCount);
		}
		MessageBox(resinfo);
		return S_OK;
	}
	LRESULT OnUploadData(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	{
		CUploadFreqData uploaddata;
		while(uploaddata.DoUpload()>0);
		return S_OK;
	}
	LRESULT OnSearchVarSite(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	{
		CSearchBySite serachbysite(freqinfos);
		if(serachbysite.StartSearch())
		{
			CAtlString resinfo;
			for(auto i=serachbysite.songresult.begin();i!=serachbysite.songresult.end();i++)
			{
				resinfo.AppendFormat(_T("song_id:%d match:%d name:%s\n"),i->song_id,i->match_count,i->filename);
			}
			MessageBox(resinfo);
		}
		return S_OK;
	}
	LRESULT OnRunFolder(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	{
		CFolderDialog dlg;
		dlg.m_bExpandInitialSelection=TRUE;
		if(IDOK!=dlg.DoModal())
			return S_OK;
		CAtlString path=dlg.m_szFolderPath;
		std::vector<CAtlString> files;
		CFindFile ff;
		if(ff.FindFile(path+L"\\*"))
		{
			do
			{
				if(ff.IsDots())
					continue;
				files.push_back(ff.GetFilePath());
			}
			while(ff.FindNextFile());
		}
		for(auto i=files.begin();i!=files.end();i++)
		{
			dataline= ReadMusicFrequencyData(*i);
			if(dataline.empty())
				continue;
			BuildData();
			CAtlString fileName=*i;
			fileName=fileName.Right(fileName.GetLength()-fileName.ReverseFind('\\')-1);
			fileName=fileName.Left(fileName.Find('.'));
			SaveMusicInfoToDb(fileName);
		}
		return S_OK;
	}
	
	bool runing;
	LRESULT OnFileRecord(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	{
		MMRESULT mmres;
		HWAVEIN hwi;
		WAVEHDR whdr,whdr1;
		UINT devId;
		WAVEFORMATEX wFormatEx;
		short outsample1[SampleCount];
		short outsample2[SampleCount];

		wFormatEx.wFormatTag = WAVE_FORMAT_PCM;
		wFormatEx.nChannels = 1;
		wFormatEx.wBitsPerSample = 16;
		wFormatEx.nSamplesPerSec = 44100;
		wFormatEx.nBlockAlign = wFormatEx.wBitsPerSample * wFormatEx.nChannels / 8;
		wFormatEx.nAvgBytesPerSec = wFormatEx.nSamplesPerSec* wFormatEx.nBlockAlign;

		// Open audio device
		debugstring(_T("\nwave in num %d\n"),waveInGetNumDevs());
		for (devId = 0; devId < waveInGetNumDevs(); devId++) {
			mmres = waveInOpen(&hwi, devId, &wFormatEx, (DWORD_PTR)CMainFrame::waveInProc,
				(DWORD_PTR)this, CALLBACK_FUNCTION);
			if (mmres == MMSYSERR_NOERROR) {
				break;
			}
		}
		if (mmres != MMSYSERR_NOERROR)
		{
			debugstring(_T("1-%d"),mmres);
			return S_FALSE;
		}

		ZeroMemory(&whdr, sizeof(WAVEHDR));
		whdr.lpData = (LPSTR)outsample1;
		whdr.dwBufferLength = sizeof(outsample1);

		ZeroMemory(&whdr1, sizeof(WAVEHDR));
		whdr1.lpData = (LPSTR)outsample2;
		whdr1.dwBufferLength = sizeof(outsample2);

		mmres = waveInPrepareHeader(hwi, &whdr, sizeof(WAVEHDR));
		if (mmres != MMSYSERR_NOERROR)
		{
			return FALSE;
		}

		mmres = waveInPrepareHeader(hwi, &whdr1, sizeof(WAVEHDR));
		if (mmres != MMSYSERR_NOERROR)
		{
			return FALSE;
		}
		
		mmres=waveInAddBuffer(hwi,&whdr,sizeof(WAVEHDR));
		mmres=waveInAddBuffer(hwi,&whdr1,sizeof(WAVEHDR));
		runing=true;
		dataline.clear();
		mmres=waveInStart(hwi);

		MessageBox(_T("点确定停止录音"));

		runing=false;
		mmres=waveInStop(hwi);
		mmres=waveInReset(hwi);
		mmres=waveInUnprepareHeader(hwi,&whdr, sizeof(WAVEHDR));
		mmres=waveInUnprepareHeader(hwi,&whdr1, sizeof(WAVEHDR));
		mmres=waveInClose(hwi);

		m_trackBar.SetRangeMax(100);
		m_trackBar.SetPos(50);
		this->BuildData();
		this->BuildImage();
		return S_OK;
	}
	static void CALLBACK waveInProc(
		HWAVEIN hwi,
		UINT uMsg,
		DWORD_PTR dwInstance,
		DWORD_PTR dwParam1,
		DWORD_PTR dwParam2
		)
	{
		CMainFrame *view=(CMainFrame*)dwInstance;
		if(view->runing && WIM_DATA==uMsg)
		{
			WAVEHDR *hdr=(WAVEHDR *)dwParam1;
			view->ProcessBuffer((short*)hdr->lpData,hdr->dwBytesRecorded/sizeof(short));
			MMRESULT mmres;
			hdr->dwFlags&=~WHDR_DONE;
			mmres = waveInAddBuffer(hwi, hdr, sizeof(WAVEHDR));
			if (mmres != MMSYSERR_NOERROR)
			{
				return ;
			}
		}
	}
	void ProcessBuffer(short *buffer,int count)
	{
		if(count!=SampleCount)
			return;
		std::vector<double> line;
		for(int i=0;i<count;i++)
			line.push_back(*(buffer+i));
		std::vector<double> outR(SampleCount),outI(SampleCount);
		fft_double(SampleCount,false,&line[0],nullptr,&outR[0],&outI[0]);
		std::vector<double> freqRes(SampleCount/2);
		for(int i=0;i<SampleCount/2;i++)
		{
			freqRes[i]=sqrt(outR[i]*outR[i]+outI[i]*outI[i]);
		}
		dataline.push_back(freqRes);
	}
	
	std::vector<std::vector<double>> darklines;
	std::vector<FreqInfo> freqinfos;
	void BuildData()
	{
		darklines.clear();
		const int checkR=2;
		for(size_t i=checkR;i!=dataline.size()-checkR;i++)
		{
			std::vector<double> line(SampleCount/2);
			for(size_t j=checkR;j<SampleCount/2-checkR;j++)
			{
				const double core1[5][5]={
					{0,-0.5,-1,-0.5,0},
					{-0.5,-1,-2,-1,-0.5},
					{-1,-2,21,-2,-1},
					{-0.5,-1,-2,-1,-0.5},
					{0,-0.5,-1,-0.5,0}
				};
				double gx=0;
				for(int testi=-checkR;testi<checkR;testi++)
				{
					for(int testj=-checkR;testj<checkR;testj++)
					{
						double v=dataline[i+testi][j+testj];
						gx+=v*core1[testi+checkR][testj+checkR];
					}
				}
				if(gx>0)
					line[j]=sqrt(gx);
			}
			darklines.push_back(std::move(line));
		}
		//darklines=dataline;
		double darkmax=0,darkmin=1e20;
		for(int i=1;i!=darklines.size()-1;i++)
		{
			for(size_t j=1;j<SampleCount/2-1;j++)
			{
				double v=darklines[i][j];
				darkmax=max(darkmax,v);
				darkmin=min(darkmin,v);
			}
		}
		double darkspan=darkmax-darkmin;
		for(int i=1;i!=darklines.size()-1;i++)
		{
			for(size_t j=1;j<SampleCount/2-1;j++)
			{
				double &v=darklines[i][j];
				v=(v-darkmin)/darkspan;
			}
		}
		
		freqinfos.clear();
		const int area=5;
		for(int i=area;i!=darklines.size()-area;i++)
		{
			for(size_t j=area;j<SampleCount/2-area;j++)
			{
				double strong=darklines[i][j];
				if(strong>0.35)
				{
					double maxstrong=strong;
					for(int x=-area;x<=area;x++)
					{
						for(int y=-area;y<=area;y++)
						{
							if(!(x==0 && y==0))
							{
								double other=darklines[i+x][j+y];
								if(other>strong)
								{
									goto NEXT;
								}
							}
						}
					}
					FreqInfo info;
					info.freq=j;
					info.time=i;
					info.strong=strong;
					freqinfos.push_back(info);
NEXT:;
				}
				//NEXT:;
			}
		}
	}
	
	void BuildImage()
	{
		/*if(memimage.IsNull()==FALSE)
			memimage.Destroy();
		
		BOOL res=memimage.CreateEx(SampleCount/2,dataline.size(),24,BI_RGB);
		CDCHandle dch=memimage.GetDC();
		CBrush srcbrush=dch.SelectBrush(NULL);
		CPen pen;
		CPen srcpen=pen.CreatePen(PS_SOLID,2,RGB(0,0,0));
		dch.SelectPen(pen);
		dch.FillSolidRect(0,0,memimage.GetWidth(),memimage.GetHeight(),RGB(255,255,255));*/
		memimage.SetBitmapSize(SampleCount/2,dataline.size());
		for(int i=0;i!=darklines.size();i++)
		{
			for(size_t j=0;j<SampleCount/2;j++)
			{
				const double back[]={255,255,255};

				double strong=darklines[i][j];
				strong=min(strong/((double)m_trackBar.GetPos()/100),1);
				memimage.SetPixel(j,i,(BYTE)(strong*255+(1-strong)*back[0]),(BYTE)((1-strong)*back[1]),(BYTE)((1-strong)*back[2]));
			}
		}

		/*for(auto i=freqinfos.begin();i!=freqinfos.end();i++)
		{
			dch.MoveTo(i->freq-3,i->time-3);
			dch.LineTo(i->freq+3,i->time+3);
			dch.MoveTo(i->freq-3,i->time+3);
			dch.LineTo(i->freq+3,i->time-3);
		}

		CPen linepen;
		linepen.CreatePen(PS_SOLID,1,RGB(0,0,255));
		dch.SelectPen(linepen);

		for(size_t j=20;j<SampleCount/2;j+=40)
		{
			dch.MoveTo(j,0);
			dch.LineTo(j,dataline.size()-1);
		}

		memimage.ReleaseDC();
		*/
		m_view.Invalidate();
		m_view.SetScrollSize(SampleCount/2,dataline.size());
	}
	void OnSize(UINT nType, CSize size)
	{
		m_view.MoveWindow(0,0,size.cx,size.cy-30);
		m_trackBar.MoveWindow(0,size.cy-30,size.cx,30);
	}
	void OnHScroll(UINT nSBCode, UINT nPos, CScrollBar pScrollBar)
	{
		if(pScrollBar.m_hWnd==m_trackBar.m_hWnd)
		{
			if(nSBCode==TB_ENDTRACK)
			{
				BuildImage();
			}
		}
	}
	LRESULT OnViewToolBar(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	{
		BOOL bVisible = !::IsWindowVisible(m_hWndToolBar);
		::ShowWindow(m_hWndToolBar, bVisible ? SW_SHOWNOACTIVATE : SW_HIDE);
		UISetCheck(ID_VIEW_TOOLBAR, bVisible);
		UpdateLayout();
		return 0;
	}

	LRESULT OnClientMouseMove(UINT /*uMsg*/, WPARAM wParam, LPARAM lParam, BOOL& /*bHandled*/)
	{
		CPoint p;
		p.x=wParam;
		p.y=lParam;
		if(p.x<SampleCount/2 && p.y<(int)darklines.size())
		{
			CAtlString str;
			str.Format(_T("%d,%d==>%.5f"),p.x,p.y,darklines[p.y][p.x]);
			SetWindowText(str);
		}
		return S_OK;
	}
};
