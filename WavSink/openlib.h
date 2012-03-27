#pragma once

#include <atlbase.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mferror.h>

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