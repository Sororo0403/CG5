#include "SoundManager.h"
#include "DxUtils.h"
#include "ResourcePath.h"
#include <cstring>
#include <filesystem>
#include <fstream>
#include <stdexcept>

using namespace DxUtils;

SoundManager::~SoundManager() {
    if (masterVoice_) {
        masterVoice_->DestroyVoice();
        masterVoice_ = nullptr;
    }
    xAudio2_.Reset();
}

void SoundManager::Initialize() {
    ThrowIfFailed(XAudio2Create(&xAudio2_, 0), "XAudio2Create failed");

    ThrowIfFailed(xAudio2_->CreateMasteringVoice(&masterVoice_),
                  "CreateMasteringVoice failed");
}

uint32_t SoundManager::Load(const std::wstring &path) {
    SoundResource res{};
    res.wav = LoadWavPcm16(path);

    sounds_.push_back(std::move(res));
    uint32_t soundId = static_cast<uint32_t>(sounds_.size() - 1);

    return soundId;
}

void SoundManager::Play(uint32_t soundId) {
    if (!xAudio2_) {
        throw std::runtime_error("SoundManager is not initialized");
    }
    if (soundId >= sounds_.size()) {
        return;
    }

    const WavData &wav = sounds_[soundId].wav;
    if (wav.buffer.empty()) {
        return;
    }

    IXAudio2SourceVoice *voice = nullptr;

    ThrowIfFailed(xAudio2_->CreateSourceVoice(
                      &voice, &wav.format, 0, XAUDIO2_DEFAULT_FREQ_RATIO,
                      &voiceCallback_),
                  "CreateSourceVoice failed");

    XAUDIO2_BUFFER buf{};
    buf.pAudioData = wav.buffer.data();
    buf.AudioBytes = static_cast<UINT32>(wav.buffer.size());
    buf.Flags = XAUDIO2_END_OF_STREAM;

    buf.pContext = voice;

    ThrowIfFailed(voice->SubmitSourceBuffer(&buf),
                  "SubmitSourceBuffer failed");

    ThrowIfFailed(voice->Start(), "SourceVoice Start failed");
}

WavData SoundManager::LoadWavPcm16(const std::wstring &path) {
    const std::filesystem::path resolvedPath =
        ResourcePath::FindExisting(std::filesystem::path(path));
    ResourcePath::RequireFile(resolvedPath, "Failed to open wav file");

    std::ifstream f(resolvedPath, std::ios::binary);
    if (!f) {
        throw std::runtime_error("Failed to open wav file: " +
                                 resolvedPath.string());
    }

    auto readExact = [&f, &resolvedPath](char *data, std::streamsize size,
                                         const char *what) {
        f.read(data, size);
        if (f.gcount() != size) {
            throw std::runtime_error(std::string("Invalid wav file while reading ") +
                                     what + ": " + resolvedPath.string());
        }
    };

    auto readU32 = [&f]() {
        uint32_t v{};
        f.read(reinterpret_cast<char *>(&v), 4);
        if (f.gcount() != 4) {
            throw std::runtime_error("Invalid wav file while reading u32");
        }
        return v;
    };
    auto readU16 = [&f]() {
        uint16_t v{};
        f.read(reinterpret_cast<char *>(&v), 2);
        if (f.gcount() != 2) {
            throw std::runtime_error("Invalid wav file while reading u16");
        }
        return v;
    };

    char riff[4]{};
    readExact(riff, 4, "RIFF header");
    if (std::memcmp(riff, "RIFF", 4) != 0) {
        throw std::runtime_error("Invalid wav file: missing RIFF header: " +
                                 resolvedPath.string());
    }

    readU32();

    char wave[4]{};
    readExact(wave, 4, "WAVE header");
    if (std::memcmp(wave, "WAVE", 4) != 0) {
        throw std::runtime_error("Invalid wav file: missing WAVE header: " +
                                 resolvedPath.string());
    }

    WavData out{};
    bool foundFmt = false;
    bool foundData = false;

    while (f && (!foundFmt || !foundData)) {
        char chunkId[4]{};
        f.read(chunkId, 4);
        if (!f)
            break;

        uint32_t chunkSize = readU32();
        if (!f)
            break;

        if (std::memcmp(chunkId, "fmt ", 4) == 0) {
            uint16_t wFormatTag = readU16();
            uint16_t nChannels = readU16();
            uint32_t nSamplesPerSec = readU32();
            uint32_t nAvgBytesPerSec = readU32();
            uint16_t nBlockAlign = readU16();
            uint16_t wBitsPerSample = readU16();

            if (wFormatTag != WAVE_FORMAT_PCM || wBitsPerSample != 16) {
                throw std::runtime_error(
                    "Unsupported wav format. Only PCM 16-bit is supported: " +
                    resolvedPath.string());
            }

            out.format.wFormatTag = wFormatTag;
            out.format.nChannels = nChannels;
            out.format.nSamplesPerSec = nSamplesPerSec;
            out.format.nAvgBytesPerSec = nAvgBytesPerSec;
            out.format.nBlockAlign = nBlockAlign;
            out.format.wBitsPerSample = wBitsPerSample;
            out.format.cbSize = 0;

            // fmt チャンク拡張分をスキップ
            const uint32_t consumed = 16;
            if (chunkSize > consumed) {
                f.seekg(chunkSize - consumed, std::ios::cur);
            }

            foundFmt = true;
        } else if (std::memcmp(chunkId, "data", 4) == 0) {
            out.buffer.resize(chunkSize);
            if (chunkSize > 0) {
                readExact(reinterpret_cast<char *>(out.buffer.data()), chunkSize,
                          "data chunk");
            }
            foundData = true;
        } else {
            f.seekg(chunkSize, std::ios::cur);
        }

        // チャンクは 2byte 境界
        if (chunkSize & 1) {
            f.seekg(1, std::ios::cur);
        }
    }

    if (!foundFmt || !foundData || out.buffer.empty()) {
        throw std::runtime_error("Invalid wav file: missing fmt/data chunk: " +
                                 resolvedPath.string());
    }

    if (out.format.nBlockAlign !=
        out.format.nChannels * (out.format.wBitsPerSample / 8)) {
        throw std::runtime_error("Invalid wav file: block alignment mismatch: " +
                                 resolvedPath.string());
    }

    return out;
}
