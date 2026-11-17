#include "Speak.h"
#include <string.h>

#include <espeak-ng/speak_lib.h>

int synthCallback(short* wav, int numsamples, espeak_EVENT* events)
{
    if (wav != nullptr && numsamples > 0) {
        size_t oldSize = Espeak::instance()->m_pcmBuffer.size();
        Espeak::instance()->m_pcmBuffer.resize(oldSize + numsamples);
        memcpy(Espeak::instance()->m_pcmBuffer.data() + oldSize, wav, numsamples * sizeof(short));
    }
    return 0;
}

Espeak* Espeak::instance()
{
    static Espeak* _ins = NULL;
    if ( _ins == NULL )
    {
        _ins = new Espeak();
    }

    return _ins;
}

Espeak::Espeak()
    : m_initialized(false)
{
}

Espeak::~Espeak()
{
    uninit();
}

bool Espeak::init(const char* dataPath, const char* voice)
{
    if (m_initialized) {
        return true;
    }

    int sampleRate = espeak_Initialize(AUDIO_OUTPUT_SYNCHRONOUS, 200, dataPath, 0);
//  printf("espeak samplerate:%d\n", sampleRate);
    if (sampleRate == -1) {
        return false;
    }

    espeak_SetSynthCallback(synthCallback);

    if (voice != nullptr && strlen(voice) > 0) {
        espeak_ERROR err = espeak_SetVoiceByName(voice);
        if (err != EE_OK) {
            espeak_Terminate();
            return false;
        }
    }
    
    espeak_VOICE spec = {};
    spec.gender = 2;
    spec.languages = voice;
    espeak_SetVoiceByProperties(&spec);

    m_initialized = true;
    return true;
}

bool Espeak::GetAudio(const char* text, char** pcm, int* len)
{
    if (!m_initialized || text == nullptr || pcm == nullptr || len == nullptr) {
        return false;
    }
    
    CCPPGuard guard(m_mutex);

    *pcm = nullptr;
    *len = 0;
    m_pcmBuffer.clear();

    espeak_ERROR err = espeak_Synth(
        text,
        strlen(text) + 1,
        0,
        POS_CHARACTER,
        0,
        espeakCHARS_AUTO | espeakENDPAUSE,
        nullptr,
        nullptr
    );

    if (err != EE_OK) {
        return false;
    }

    espeak_Synchronize();

    if (m_pcmBuffer.empty()) {
        return false;
    }

    short* outPcm = nullptr;
    int outLen = 0;

//  FILE* fp = NULL;
//  fopen_s(&fp, "tmp.pcm", "ab+");
//  fwrite((void*)m_pcmBuffer.data(), m_pcmBuffer.size(), 1, fp);
//  fclose(fp);

    outLen = resampleTo16k(m_pcmBuffer.data(), (int)m_pcmBuffer.size(), 22050, &outPcm);
    if (outLen <= 0 || outPcm == nullptr) {
        return false;
    }

    *pcm = (char*)outPcm;
    *len = outLen*2;
    
    return true;
}

void Espeak::uninit()
{
    if (m_initialized) {
        espeak_Terminate();
        m_initialized = false;
    }
    m_pcmBuffer.clear();
}

int Espeak::resampleTo16k(const short* src, int srcCount, int srcRate, short** dst)
{
    if (src == nullptr || srcCount <= 0 || srcRate <= 0 || dst == nullptr) {
        return 0;
    }

    const int dstRate = 16000;

    // 向上取整多留 1 个样本防溢出
    long long tmpLen = (long long)srcCount * dstRate / srcRate + 2;
    if (tmpLen <= 0 || tmpLen > (long long)srcCount * 8) {
        return 0;
    }

    int dstCount = (int)tmpLen;
    short* outBuf = (short*)malloc(dstCount * sizeof(short));
    if (outBuf == nullptr) {
        return 0;
    }

    // 线性插值重采样
    for (int i = 0; i < dstCount; ++i) {
        long long posNumer = (long long)i * srcRate;
        int idx0 = (int)(posNumer / dstRate);
        int idx1 = idx0 + 1;
        if (idx0 < 0) idx0 = 0;
        if (idx1 < 0) idx1 = 0;
        if (idx0 >= srcCount) idx0 = srcCount - 1;
        if (idx1 >= srcCount) idx1 = srcCount - 1;

        long long frac = posNumer - (long long)idx0 * dstRate; // 0..dstRate
        long long s0 = src[idx0];
        long long s1 = src[idx1];
        long long v  = s0 + ((s1 - s0) * frac) / dstRate;

        if (v > 32767)  v = 32767;
        if (v < -32768) v = -32768;
        outBuf[i] = (short)v;
    }

    *dst = outBuf;
    return dstCount;
}

