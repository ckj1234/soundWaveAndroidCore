#include <windows.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mferror.h>
#include <stdio.h>
#include <atlbase.h>

#include "..\\WavSink\\CreateWavSink.h"
#include "..\\WavSink\\WavSink.h"

std::vector<std::vector<double>> CreateWavFile(const WCHAR *sURL);

HRESULT CreateMediaSource(const WCHAR *sURL, IMFMediaSource **ppSource);
HRESULT CreateTopology(IMFMediaSource *pSource, IMFMediaSink *pSink, IMFTopology **ppTopology);

HRESULT CreateTopologyBranch(
    IMFTopology *pTopology,
    IMFMediaSource *pSource,          // Media source.
    IMFPresentationDescriptor *pPD,   // Presentation descriptor.
    IMFStreamDescriptor *pSD,         // Stream descriptor.
    IMFMediaSink *pSink
    );

HRESULT CreateSourceNode(
    IMFMediaSource *pSource,          // Media source.
    IMFPresentationDescriptor *pPD,   // Presentation descriptor.
    IMFStreamDescriptor *pSD,         // Stream descriptor.
    IMFTopologyNode **ppNode          // Receives the node pointer.
    );

HRESULT CreateOutputNode(IMFMediaSink *pSink, DWORD iStream, IMFTopologyNode **ppNode);

HRESULT RunMediaSession(IMFTopology *pTopology);


///////////////////////////////////////////////////////////////////////
//  Name: wmain
//  Description:  Entry point to the application.
//
//  Usage: writewavfile.exe inputfile outputfile
///////////////////////////////////////////////////////////////////////

int wmain(int argc, wchar_t *argv[ ])
{
    HRESULT hr;

    if (argc != 3)
    {
        wprintf(L"Usage: WriteWavFile.exe InputFile OuputFile\n");
        return -1;
    }

    hr = MFStartup(MF_VERSION);
    if (FAILED(hr))
    {
        wprintf(L"MFStartup failed!\n");
    }

    if (SUCCEEDED(hr))
    {
        std::vector<std::vector<double>> res = CreateWavFile(argv[1]);

		if(!res.empty())
        {
            wprintf(L"Done!\n");
        }
        else
        {
            wprintf(L"Error: Unable to author file.\n");
        }
    }

    MFShutdown();

    return 0;
}




///////////////////////////////////////////////////////////////////////
//  Name: CreateWavFile
//  Description:  Creates a .wav file from an input file.
///////////////////////////////////////////////////////////////////////

std::vector<std::vector<double>> CreateWavFile(const WCHAR *sURL)
{
    //CComPtr<IMFByteStream> pStream;
    CComPtr<IMFMediaSink> pSink;
    CComPtr<IMFMediaSource> pSource;
    CComPtr<IMFTopology> pTopology;
	CComPtr<IWaveDataRecorder> waveRecord;
	HRESULT hr=0;
	hr=CWavRecord::CreateInstanse(&waveRecord);
   // hr = MFCreateFile(MF_ACCESSMODE_WRITE, MF_OPENMODE_DELETE_IF_EXIST, MF_FILEFLAGS_NONE, L"D:\\out.wav", &pStream);
    if (FAILED(hr))
    {
        wprintf(L"MFCreateFile failed!\n");
    }

    // Create the WavSink object.
    if (SUCCEEDED(hr))
    {
        hr = CreateWavSink(NULL,waveRecord, &pSink);
    }

    // Create the media source from the URL.
    if (SUCCEEDED(hr))
    {
        hr = CreateMediaSource(sURL, &pSource);
    }

    // Create the topology.
    if (SUCCEEDED(hr))
    {
        hr = CreateTopology(pSource, pSink, &pTopology);
    }

    // Run the media session.
    if (SUCCEEDED(hr))
    {
        hr = RunMediaSession(pTopology);
        if (FAILED(hr))
        {
            wprintf(L"RunMediaSession failed!\n");
        }
    }

    if (pSource)
    {
        pSource->Shutdown();
    }

	std::vector<std::vector<double>> data;
	waveRecord->PullOutData(&data);
    return data;
}