# AudioToText

A simple C++ command‑line tool that converts WAV audio files into text using Microsoft Speech API (SAPI).

## Features

- **Free‑form dictation**: Utilizes SAPI's built‑in dictation grammar for accurate speech‑to‑text.
- **WAV support**: Automatically handles PCM WAV files via SAPI stream binding.
- **UTF‑8 output**: Converts wide strings to UTF‑8 for cross‑platform compatibility.
- **Robust error handling**: Checks all COM and SAPI calls and reports failures.

## Prerequisites

- Windows OS (Windows 7 or later)
- Visual Studio (2015+) or MSVC Build Tools
- Windows SDK with SAPI headers

## Building

Open a developer command prompt and run:

```bat
cl /EHsc /nologo AudioToText.cpp /link sapi.lib
```

This compiles `AudioToText.cpp` and links against `sapi.lib`.

## Usage

```bat
AudioToText.exe <input_wav_file> <output_txt_file>
```

- `<input_wav_file>`: Path to a 16‑bit PCM WAV file containing speech.
- `<output_txt_file>`: Path where the transcribed text will be saved.

Example:

```bat
AudioToText.exe samples\interview.wav output\transcript.txt
```

When successful, the program prints:

```
Conversion complete. Output written to output/transcript.txt
```

## How It Works

1. **COM initialization**: Initializes the COM library with `CoInitializeEx`.
2. **Recognizer setup**: Creates an in‑process SAPI recognizer (`SpInprocRecognizer`).
3. **Grammar**: Loads and activates the dictation grammar for free‑form speech.
4. **Audio input**: Binds the WAV file to an `ISpStream` via `SPBindToFile`.
5. **Recognition loop**: Reads `SPEI_RECOGNITION` events, extracts text, and appends to the output buffer.
6. **Cleanup**: Deactivates dictation and uninitializes COM.

## Extending

- **Custom grammars**: Instead of dictation, load a custom grammar `.cfg` file.
- **Language support**: Configure `ISpRecognizer` for different locales.
- **Real‑time input**: Change `SetInput` to use a microphone.

## License

This project is released under the MIT License. See [LICENSE](LICENSE) for details.

---

*Forward‑thinking by leveraging built‑in Windows APIs to get you up and running quickly.*

