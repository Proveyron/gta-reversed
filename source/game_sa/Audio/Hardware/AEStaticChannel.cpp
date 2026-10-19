#include "StdInc.h"

#include "AEStaticChannel.h"

#include "AESmoothFadeThread.h"

void CAEStaticChannel::InjectHooks() {
    RH_ScopedVirtualClass(CAEStaticChannel, 0x85F3CC, 9);
    RH_ScopedCategory("Audio/Hardware");

    RH_ScopedVMTInstall(Service, 0x4F10D0);
    RH_ScopedVMTInstall(IsSoundPlaying, 0x4F0F40);
    RH_ScopedVMTInstall(GetPlayTime, 0x4F0F70);
    RH_ScopedVMTInstall(GetLength, 0x4F0FA0);
    RH_ScopedVMTInstall(Play, 0x4F0BD0);
    RH_ScopedVMTInstall(SynchPlayback, 0x4F1040);
    RH_ScopedVMTInstall(Stop, 0x4F0FB0);

    RH_ScopedInstall(SetAudioBuffer, 0x4F0C40);
}

CAEStaticChannel::CAEStaticChannel(IDirectSound* pDirectSound, uint16 channelId, bool hardwareMixAvailable, uint32 samplesPerSec, uint16 bitsPerSample) :
    CAEAudioChannel(pDirectSound, channelId, samplesPerSec, bitsPerSample),
    m_bNeedData(false),
    m_bNeedsSynch(false),
    m_IsHardwareMixAvailable(hardwareMixAvailable)
{
}

// 0x4F10D0
void CAEStaticChannel::Service() {
    if (!m_pDirectSoundBuffer) {
        m_nBufferStatus = 0;
        return;
    }

    if (m_bNeedData && (int32)(CTimer::GetTimeInMS() - m_nSyncTime) > field_74) {
        uint8* ppvAudioPtr1{};
        DWORD pdwAudioBytes{};

        VERIFY(SUCCEEDED(m_pDirectSoundBuffer->Lock(
            m_dwLockOffset,
            m_nNumLockBytes,
            reinterpret_cast<LPVOID*>(&ppvAudioPtr1),
            &pdwAudioBytes,
            nullptr,
            0,
            0
        )));

        for (auto i = 0u; i < m_nNumLoops; i++) {
            memcpy(
                &ppvAudioPtr1[i * m_nNumLockBytes],
                (uint8*)m_pBuffer + m_nCurrentBufferOffset,
                m_nNumLockBytes
            );
        }
        VERIFY(SUCCEEDED(m_pDirectSoundBuffer->Unlock(ppvAudioPtr1, pdwAudioBytes, nullptr, 0)));
        m_bNeedData = false;
    }

    UpdateStatus();

    if (!m_bPaused && !bufferStatus.Bit0x1) {
        if (const auto buf = std::exchange(m_pDirectSoundBuffer, nullptr)) {
            --g_numSoundChannelsUsed;
            buf->Release();
        }
    }
}

// 0x4F0F40
bool CAEStaticChannel::IsSoundPlaying() {
    if (!m_pDirectSoundBuffer)
        return false;

    if (m_bPaused || m_bNeedsSynch)
        return true;

    return CAEAudioChannel::IsBufferPlaying();
}

// 0x4F0F70
int16 CAEStaticChannel::GetPlayTime() {
    if (!IsSoundPlaying())
        return -1;

    const auto curPos = CAEAudioChannel::GetCurrentPlaybackPosition();
    return CAEAudioChannel::ConvertFromBytesToMS(curPos);
}

// 0x4F0FA0
uint16 CAEStaticChannel::GetLength() {
    return CAEAudioChannel::ConvertFromBytesToMS(m_nLengthInBytes);
}

// 0x4F0BD0
void CAEStaticChannel::Play(int16 timeInMs, int8 unused, float scalingFactor) {
    if (m_bLooped && m_nCurrentBufferOffset != 0 || !timeInMs) {
        m_bUnkn2 = false;
    } else {
        m_pDirectSoundBuffer->SetCurrentPosition(ConvertFromMsToBytes(timeInMs));
        m_bUnkn2 = true;
    }
    m_bNeedsSynch = true;
    m_bPaused = scalingFactor == 0.0f;
}

    

// 0x4F1040
void CAEStaticChannel::SynchPlayback() {
    if (!m_pDirectSoundBuffer || !m_bNeedsSynch || m_bPaused)
        return;

    if (m_bUnkn2) {
        m_pDirectSoundBuffer->SetVolume(-10000);
        if (!AESmoothFadeThread.RequestFade(m_pDirectSoundBuffer, m_Volume, -2, false)) {
            const auto dwVolume = static_cast<LONG>(m_Volume * 100.0F);
            m_pDirectSoundBuffer->SetVolume(dwVolume);
        }
    }

    m_pDirectSoundBuffer->Play(0, 0, m_bLooped);
    m_nSyncTime = CTimer::GetTimeInMS();
    m_bNeedsSynch = false;
}

// 0x4F0FB0
void CAEStaticChannel::Stop() {
    if (m_pDirectSoundBuffer &&
        CAEAudioChannel::IsBufferPlaying() &&
        !AESmoothFadeThread.RequestFade(m_pDirectSoundBuffer, -100.0F, -1, true)
    ) {
        m_pDirectSoundBuffer->Stop();
    }

    { // todo: Same as CAEAudioChannel::~CAEAudioChannel
    if (m_pDirectSoundBuffer) {
        --g_numSoundChannelsUsed;
        m_pDirectSoundBuffer->Release();
        m_pDirectSoundBuffer = nullptr;
    }

    if (m_pDirectSound3DBuffer) {
        m_pDirectSound3DBuffer->Release();
        m_pDirectSound3DBuffer = nullptr;
    }
    }
}

// 0x4F0C40
bool CAEStaticChannel::SetAudioBuffer(IDirectSound3DBuffer* buffer, uint16 size, int16 f88, int16 f8c, int16 loopOffset, uint16 frequency) {
#ifdef USE_DSOUND
    if (!size || !frequency) {
        return false;
    }

    if (m_pDirectSoundBuffer) {
        --g_numSoundChannelsUsed;
        m_pDirectSoundBuffer->Release();
        m_pDirectSoundBuffer = nullptr;
    }
    if (m_pDirectSound3DBuffer) {
        m_pDirectSound3DBuffer->Release();
        m_pDirectSound3DBuffer = nullptr;
    }

    m_pBuffer = buffer;
    m_bLooped = false;
    m_bNeedData = false;
    m_bPaused = false;
    m_nCurrentBufferOffset = 0;
    field_68 = 0;
    field_6C = 0;
    m_nLengthInBytes = size;
    field_88 = f88;
    field_8C = f8c;

    if (loopOffset != -1) {
        m_bLooped = true;
        m_nCurrentBufferOffset = (int32)loopOffset * 2;
        field_68 = size;
    }

    if (m_bLooped && m_nCurrentBufferOffset != 0) {
        const auto loopLen = (uint32)(field_68 - m_nCurrentBufferOffset);
        m_nNumLockBytes = loopLen;
        const auto minBytes = std::max((uint32)field_68, 24000u);
        field_6C = (minBytes / loopLen) + 1;
        m_nBufferBytes = field_6C * loopLen;
    } else {
        m_nBufferBytes = size;
    }

    m_nFrequency = frequency;
    m_nOriginalFrequency = frequency;
    m_WaveFormat.wFormatTag = WAVE_FORMAT_PCM;
    m_WaveFormat.nChannels = 1;
    m_WaveFormat.nSamplesPerSec = frequency;
    m_WaveFormat.nAvgBytesPerSec = frequency * 2;
    m_WaveFormat.nBlockAlign = 2;
    m_WaveFormat.wBitsPerSample = 16;
    m_WaveFormat.cbSize = 0;

    DSBUFFERDESC dsbdesc{};
    dsbdesc.dwSize = sizeof(DSBUFFERDESC);
    dsbdesc.dwFlags = (m_IsHardwareMixAvailable ? 0x80b4 : 0x80b8);
    dsbdesc.dwBufferBytes = m_nBufferBytes;
    dsbdesc.lpwfxFormat = &m_WaveFormat;

    HRESULT hr = m_pDirectSound->CreateSoundBuffer(&dsbdesc, &m_pDirectSoundBuffer, nullptr);
    if (FAILED(hr)) {
        return false;
    }

    g_numSoundChannelsUsed++;

    LPVOID audioPtr1 = nullptr;
    DWORD audioBytes1 = 0;
    hr = m_pDirectSoundBuffer->Lock(0, m_nLengthInBytes, &audioPtr1, &audioBytes1, nullptr, nullptr, DSBLOCK_ENTIREBUFFER);
    if (FAILED(hr)) {
        if (m_pDirectSoundBuffer) {
            m_pDirectSoundBuffer->Release();
            m_pDirectSoundBuffer = nullptr;
        }
        return false;
    }

    uint32 playPos = 0;
    if (m_nCurrentBufferOffset == 0) {
        memcpy(audioPtr1, m_pBuffer, size);
        if (size < m_nLengthInBytes) {
            memset((uint8*)audioPtr1 + size, 0, m_nLengthInBytes - size);
        }
        m_bNeedData = false;
        playPos = 0;
    } else {
        playPos = m_nBufferBytes - m_nCurrentBufferOffset;
        memcpy((uint8*)audioPtr1 + playPos, m_pBuffer, m_nCurrentBufferOffset);

        const auto loopLen = (uint32)m_nNumLockBytes;
        m_nNumLoops = (m_nCurrentBufferOffset / loopLen) + 1;
        m_dwLockOffset = m_nBufferBytes - (m_nNumLoops * loopLen);

        const auto numCopies = field_6C - m_nNumLoops;
        for (auto i = 0u; i < numCopies; i++) {
            memcpy((uint8*)audioPtr1 + i * loopLen, (uint8*)m_pBuffer + m_nCurrentBufferOffset, loopLen);
        }

        field_74 = (int32)ConvertFromBytesToMS(m_nCurrentBufferOffset);
        m_bNeedData = true;
    }

    m_pDirectSoundBuffer->Unlock(audioPtr1, audioBytes1, nullptr, 0);
    m_pDirectSoundBuffer->SetCurrentPosition(playPos);
    m_pDirectSoundBuffer->QueryInterface(IID_IDirectSound3DBuffer, (void**)&m_pDirectSound3DBuffer);

    m_Volume = -100.0f;
    m_pDirectSoundBuffer->SetVolume(DSBVOLUME_MIN);

    return true;
#else
    return false;
#endif
}
