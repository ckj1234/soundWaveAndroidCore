#pragma once

struct SongResult
{
	int song_id;
	int match_count;
	int maxposcount;
	CAtlString filename;
};
class CSearchBySite
{
	std::vector<FreqInfo> freqlist;
public:
	std::vector<SongResult> songresult;
	CSearchBySite(std::vector<FreqInfo> &freqdatatopost);
	~CSearchBySite(void);

	bool StartSearch();
};

