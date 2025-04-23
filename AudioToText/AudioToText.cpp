// AudioToText.cpp
#include <windows.h>
#include <sapi.h>
#include <sphelper.h>
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <atlbase.h>      // For CComPtr
#pragma comment(lib, "sapi.lib")

// Helper: convert wide‐string to UTF8
static std::string WideToUTF8(const std::wstring& w)
{
    if (w.empty()) return {};
    int sizeNeeded = ::WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(),
                                           NULL, 0, NULL, NULL);
    std::string s(sizeNeeded, 0);
    ::WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(),
                          &s[0], sizeNeeded, NULL, NULL);
    return s;
}

bool RecognizeWavFile(const std::wstring& wavPath, std::string& outText)
{
    HRESULT hr = S_OK;

    // 1) Init COM
    hr = ::CoInitializeEx(NULL, COINIT_MULTITHREADED);
    if (FAILED(hr)) {
        std::cerr << "CoInitializeEx failed: 0x" << std::hex << hr << std::endl;
        return false;
    }

    // 2) Create in-proc recognizer
    CComPtr<ISpRecognizer> cpRecognizer;
    hr = cpRecognizer.CoCreateInstance(CLSID_SpInprocRecognizer);
    if (FAILED(hr)) {
        std::cerr << "Failed to create recognizer: 0x" << std::hex << hr << std::endl;
        ::CoUninitialize();
        return false;
    }

    // 3) Create recognition context
    CComPtr<ISpRecoContext> cpContext;
    hr = cpRecognizer->CreateRecoContext(&cpContext);
    if (FAILED(hr)) {
        std::cerr << "CreateRecoContext failed: 0x" << std::hex << hr << std::endl;
        ::CoUninitialize();
        return false;
    }

    // 4) Set up a grammar for free‐form dictation
    CComPtr<ISpRecoGrammar> cpGrammar;
    hr = cpContext->CreateGrammar(1, &cpGrammar);
    if (FAILED(hr)) {
        std::cerr << "CreateGrammar failed: 0x" << std::hex << hr << std::endl;
        ::CoUninitialize();
        return false;
    }

    hr = cpGrammar->LoadDictation(NULL, SPLO_STATIC);
    if (FAILED(hr)) {
        std::cerr << "LoadDictation failed: 0x" << std::hex << hr << std::endl;
        ::CoUninitialize();
        return false;
    }

    hr = cpGrammar->SetDictationState(SPRS_ACTIVE);
    if (FAILED(hr)) {
        std::cerr << "SetDictationState failed: 0x" << std::hex << hr << std::endl;
        ::CoUninitialize();
        return false;
    }

    // 5) Open the WAV file as a SAPI audio input
    CComPtr<ISpStream> cpFileStream;
    hr = SPBindToFile(wavPath.c_str(), SPFM_OPEN_READONLY, &cpFileStream, &SPDFID_WaveFormatEx, NULL);
    if (FAILED(hr)) {
        std::cerr << "SPBindToFile failed (cannot open WAV?): 0x" << std::hex << hr << std::endl;
        ::CoUninitialize();
        return false;
    }
    hr = cpRecognizer->SetInput(cpFileStream, TRUE);
    if (FAILED(hr)) {
        std::cerr << "SetInput failed: 0x" << std::hex << hr << std::endl;
        ::CoUninitialize();
        return false;
    }

    // 6) Listen for recognition events until EOF
    CSpEvent event;
    while (event.GetFrom(cpContext) == S_OK) {
        if (event.eEventId == SPEI_RECOGNITION) {
            ISpRecoResult* pResult = event.RecoResult();
            if (pResult) {
                WCHAR* pwszText = nullptr;
                hr = pResult->GetText(SP_GETWHOLEPHRASE, SP_GETWHOLEPHRASE, FALSE, &pwszText, nullptr);
                if (SUCCEEDED(hr) && pwszText) {
                    outText += WideToUTF8(pwszText);
                    outText += "\n";
                    ::CoTaskMemFree(pwszText);
                }
            }
        }
    }

    // 7) Clean up
    cpGrammar->SetDictationState(SPRS_INACTIVE);
    ::CoUninitialize();
    return !outText.empty();
}

int wmain(int argc, wchar_t* argv[])
{
    if (argc != 3) {
        std::wcerr << L"Usage: " << argv[0]
                   << L" <input_wav_file> <output_txt_file>\n";
        return 1;
    }

    std::wstring wavFile = argv[1];
    std::wstring txtFile = argv[2];
    std::string recognized;

    if (!RecognizeWavFile(wavFile, recognized)) {
        std::cerr << "Failed to recognize speech or no text found.\n";
        return 1;
    }

    // Write out
    std::ofstream ofs(txtFile, std::ios::out | std::ios::binary);
    if (!ofs) {
        std::cerr << "Cannot open output file: "
                  << WideToUTF8(txtFile) << std::endl;
        return 1;
    }
    ofs << recognized;
    ofs.close();

    std::cout << "Conversion complete. Output written to "
              << WideToUTF8(txtFile) << std::endl;
    return 0;
}
