#pragma once
class CUploadFreqData
{
private:
	CSqlite db;
	CSqliteStmt readOption;
	CSqliteStmt writeOption;
	CSqliteStmt getNextSongToUpload;
	CSqliteStmt getAnchorCheckPoint;
public:
	CUploadFreqData(void);

	int DoUpload();
};

