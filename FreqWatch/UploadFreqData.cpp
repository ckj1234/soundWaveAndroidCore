#include "StdAfx.h"
#include "UploadFreqData.h"


CUploadFreqData::CUploadFreqData(void)
{
	db.Open(L"D:\\freq_info.data.db");
	int res=db.Execute(L"CREATE TABLE WorkOption (name  TEXT(64),value  INTEGER,PRIMARY KEY (name))");
	readOption=db.Prepare(L"select value from WorkOption where name=?1");
	writeOption=db.Prepare(L"replace into WorkOption(name,value) values(?1,?2)");
	getNextSongToUpload=db.Prepare(L"select id,name from songlist where id>?1 order by id limit 1");
	getAnchorCheckPoint=db.Prepare(L"select anchor_time,anchor_freq,check_freq,time_offset from Anchor_Check_list where song_id=?1");
}

struct AnchorInfo
{
	int anchor_time,anchor_freq,check_freq,time_offset;
};
int CUploadFreqData::DoUpload()
{
	int songid=0;
	CAtlString songname;
	readOption.Bind(1,L"lastUploadSong");
	if(readOption.Step()==SQLITE_ROW)
	{
		songid=readOption.GetInt(0);
	}
	readOption.Reset();

	getNextSongToUpload.Bind(1,songid);
	songid=0;
	if(getNextSongToUpload.Step()==SQLITE_ROW)
	{
		songid=getNextSongToUpload.GetInt(0);
		songname=CA2W(getNextSongToUpload.GetText(1),CP_UTF8);
	}
	getNextSongToUpload.Reset();
	if(songid==0)
		return 0;
	std::vector<AnchorInfo> anchorlist;
	getAnchorCheckPoint.Bind(1,songid);
	while(getAnchorCheckPoint.Step()==SQLITE_ROW)
	{
		AnchorInfo info;
		info.anchor_time=getAnchorCheckPoint.GetInt(0);
		info.anchor_freq=getAnchorCheckPoint.GetInt(1);
		info.check_freq=getAnchorCheckPoint.GetInt(2);
		info.time_offset=getAnchorCheckPoint.GetInt(3);

		anchorlist.push_back(std::move(info));
	}
	getAnchorCheckPoint.Reset();

	json2::CJsonObject mainobj;
	mainobj.Put(L"song",songname);
	json2::PJsonArray AnchorList=new json2::CJsonArray();
	mainobj.PutValue(L"anchors",AnchorList);

	for(auto i=anchorlist.begin();i!=anchorlist.end();++i)
	{
		json2::PJsonObject anchorObject=new json2::CJsonObject();
		AnchorList->PutValue(anchorObject);
		anchorObject->Put(L"anchor_time",i->anchor_time);
		anchorObject->Put(L"anchor_freq",i->anchor_freq);
		anchorObject->Put(L"check_freq",i->check_freq);
		anchorObject->Put(L"time_offset",i->time_offset);
	}

	GZipOutput strout;
	mainobj.Save(strout);
	strout.Flush();

	CSockAddr saddr(L"liveplustest.sinaapp.com",L"80");
	HTTPClient::SocketEx socket;

	HTTPClient::HttpRequest request("POST","/music/UploadSongAnchorCheck.php");
	request.setHost("liveplustest.sinaapp.com");
	request.setContentType("gzip/json");
	request.setContentLength(strout.memstream->GetBufferSize());
	int uploaded_songid=0;
	if(0==socket.Connect(saddr.Addr(),saddr.Len()))
	{
		request.SendRequest(&socket);
		socket.Send((const char*)strout.memstream->GetBuffer(),strout.memstream->GetBufferSize());

		HTTPClient::HttpResponse response;
		if(response.RecvResponseHead(&socket))
		{
			CComPtr<IMemoryStream> memstream;
			CComQIPtr<IStream> outstream;
			ByteStream::CreateInstanse(&memstream);
			outstream=memstream;
			response.RecvResponseBody(&socket,outstream);

			json2::PJsonObject resobj;
			resobj=json2::CJsonReader::Prase((LPCSTR)memstream->GetBuffer(),(unsigned int)memstream->GetBufferSize());
			int server_errno=resobj->GetNumber(L"errno");
			CAtlString error=resobj->GetString("error");
			if(server_errno==0)
			{
				json2::PJsonObject data=resobj->Get(L"data");
				int server_song_id=data->GetNumber(L"song_id");
				CAtlString server_song_name=data->GetString(L"song_name");

				if(server_errno==0)
				{
					uploaded_songid=songid;
					writeOption.Bind(1,L"lastUploadSong");
					writeOption.Bind(2,songid);
					writeOption.Step();
					writeOption.Reset();
				}
			}
		}
	}
	
	return uploaded_songid;
}