// AudioToText.cpp
#include <windows.h>
#include <sapi.h>
#include <sphelper.h>
#include <atlbase.h>      // For CComPtr
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <stdexcept>

#pragma comment(lib, "sapi.lib")

// Macro to check HRESULT and throw on failure
#define CHECK_HR(expr, msg)                                      \
    do {                                                         \
        HRESULT _hr = (expr);                                    \
        if (FAILED(_hr)) {                                       \
            std::ostringstream _oss;                             \
            _oss << msg << " (0x" << std::hex << _hr << ")";     \
            throw std::runtime_error(_oss.str());                \
        }                                                        \
    } while (0)

// RAII for COM init/uninit
struct ComInitializer {
    ComInitializer() {
        CHECK_HR(::CoInitializeEx(nullptr, COINIT_MULTITHREADED),
                 "Failed to initialize COM");
    }
    ~ComInitializer() {
        ::CoUninitialize();
    }
};

// Convert wide string (UTF-16) to UTF-8
static std::string WideToUTF8(const std::wstring& w) {
    if (w.empty()) return {};
    int size = ::WideCharToMultiByte(
        CP_UTF8, 0, w.data(), (int)w.size(), NULL, 0, NULL, NULL
    );
    std::string s(size, '\0');
    ::WideCharToMultiByte(
        CP_UTF8, 0, w.data(), (int)w.size(), &s[0], size, NULL, NULL
    );
    return s;
}

class SpeechRecognizer {
public:
    SpeechRecognizer(bool verbose = false)
        : verbose_(verbose)
    {
        // 1) Create in-proc recognizer
        CHECK_HR(recognizer_.CoCreateInstance(CLSID_SpInprocRecognizer),
                 "Could not create SAPI in-proc recognizer");

        // 2) Create a recognition context
        CHECK_HR(recognizer_->CreateRecoContext(&context_),
                 "Could not create recognition context");

        // 3) Set up a dictation grammar
        CHECK_HR(context_->CreateGrammar(GRAMMAR_ID, &grammar_),
                 "Could not create grammar");
        CHECK_HR(grammar_->LoadDictation(nullptr, SPLO_STATIC),
                 "Could not load dictation");
    }

    // Recognize speech in `wavPath` and return the full transcript
    std::string recognizeFromFile(const std::wstring& wavPath) {
        if (verbose_) std::cerr << "[+] Opening WAV file: " << WideToUTF8(wavPath) << "\n";

        // Bind the WAV file as input
        CComPtr<ISpStream> fileStream;
        CHECK_HR(SPBindToFile(
                    wavPath.c_str(),
                    SPFM_OPEN_READONLY,
                    &fileStream,
                    &SPDFID_WaveFormatEx,
                    nullptr),
                 "Failed to bind WAV file");

        CHECK_HR(recognizer_->SetInput(fileStream, TRUE),
                 "Failed to set recognizer input");

        // Activate dictation
        CHECK_HR(grammar_->SetDictationState(SPRS_ACTIVE),
                 "Could not activate dictation");

        // Event loop: collect RESULTS and exit on END_SR_STREAM
        CSpEvent event;
        std::string result;
        while (true) {
            HRESULT hr = event.GetFrom(context_);
            if (FAILED(hr)) break;

            switch (event.eEventId) {
            case SPEI_RECOGNITION:
                extractText(event.RecoResult(), result);
                if (verbose_) std::cerr << "[*] Recognition event received\n";
                break;

            case SPEI_END_SR_STREAM:
                if (verbose_) std::cerr << "[*] End of stream\n";
                goto cleanup;
            default:
                break;
            }
        }

    cleanup:
        // Deactivate dictation
        grammar_->SetDictationState(SPRS_INACTIVE);
        return result;
    }

private:
    void extractText(ISpRecoResult* pResult, std::string& out) {
        if (!pResult) return;
        WCHAR* pText = nullptr;
        HRESULT hr = pResult->GetText(
            SP_GETWHOLEPHRASE, SP_GETWHOLEPHRASE, FALSE, &pText, nullptr
        );
        if (SUCCEEDED(hr) && pText) {
            out += WideToUTF8(pText);
            out += "\n";
            ::CoTaskMemFree(pText);
        }
    }

    static constexpr ULONGLONG GRAMMAR_ID = 1;
    CComPtr<ISpRecognizer>   recognizer_;
    CComPtr<ISpRecoContext>  context_;
    CComPtr<ISpRecoGrammar>  grammar_;
    bool                     verbose_;
};

int wmain(int argc, wchar_t* argv[]) {
    bool verbose = false;
    int argi = 1;

    // Simple flag parsing
    if (argi < argc && std::wstring(argv[argi]) == L"-v") {
        verbose = true;
        ++argi;
    }

    if (argc - argi != 2) {
        std::wcerr << L"Usage: " << argv[0]
                   << L" [-v] <input_wav_file> <output_txt_file|- for stdout>\n";
        return 1;
    }

    std::wstring inputPath  = argv[argi++];
    std::wstring outputPath = argv[argi];

    try {
        ComInitializer com;
        SpeechRecognizer recognizer(verbose);
        auto transcript = recognizer.recognizeFromFile(inputPath);

        // Output
        if (outputPath == L"-") {
            std::cout << transcript;
        } else {
            std::ofstream ofs(outputPath, std::ios::binary);
            if (!ofs) throw std::runtime_error("Cannot open output file");
            ofs << transcript;
        }

        if (verbose) {
            std::cerr << "[+] Successfully wrote transcript to "
                      << WideToUTF8(outputPath) << "\n";
        } else {
            std::cout << "Done.\n";
        }
    }
    catch (const std::exception& ex) {
        std::cerr << "Error: " << ex.what() << "\n";
        return 1;
    }

    return 0;
}
