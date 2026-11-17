#include "portaudio.h"
#include "AudioDevice.h"
#include "CPPPacket.h"

static int MicDataCallBack(const void* inputBuffer, void* outputBuffer,
	unsigned long framesPerBuffer,
	const PaStreamCallbackTimeInfo* timeInfo,
	PaStreamCallbackFlags statusFlags,
	void* userData)
{
	AudioDevice* ap = (AudioDevice*)userData;
	stAPParam* micParam = ap->GetMicParam();

    if(framesPerBuffer != micParam->framesPerBuffer)
    {
        printf("MicDataCallBack is err:%d %d\n", framesPerBuffer, micParam->framesPerBuffer);
        return paContinue;
    }
    
    ap->AudioDataSend(inputBuffer);

	return paContinue;
}

static int PlayDataCallback(const void *inputBuffer, void *outputBuffer,
    unsigned long framesPerBuffer,
    const PaStreamCallbackTimeInfo* timeInfo,
    PaStreamCallbackFlags statusFlags,
    void *userData)
{
	AudioDevice* ap = (AudioDevice*)userData;
	stAPParam* playParam = ap->GetPlayParam();
    if(framesPerBuffer != playParam->framesPerBuffer)
    {
        printf("PlayDataCallback is err:%d %d\n", framesPerBuffer, playParam->framesPerBuffer);
        memset(outputBuffer, 0, FRAMES_PER_BUFFER*FORMAT/8);
        return paContinue;
    }
    
    CPacket* pPkt = ap->GetPlayAudio();
    if(pPkt == NULL)
    {
        memset(outputBuffer, 0, FRAMES_PER_BUFFER*FORMAT/8);
        return paContinue;
    }

    if(pPkt->FrameGetLength() == 0)
    {
        CPPRelease(pPkt);
        memset(outputBuffer, 0, FRAMES_PER_BUFFER*FORMAT/8);
        return paContinue;
    }

//  printf("play data\n");
//  printf("len:%d\n", pPkt->FrameGetLength());
//  FILE *pcm_out = fopen("tt.pcm","ab+");
//  fwrite(pPkt->FrameGetData(), 1, pPkt->FrameGetLength(), pcm_out);
//  fclose(pcm_out);
    
    memcpy(outputBuffer, pPkt->FrameGetData(), pPkt->FrameGetLength());
    CPPRelease(pPkt);
    
	return paContinue;
}

AudioDevice* AudioDevice::instance()
{
    static AudioDevice* _ins = NULL;
    if ( _ins == NULL )
    {
        _ins = new AudioDevice();
    }

    return _ins;
}

AudioDevice::AudioDevice()
{
    m_globalInit = 0;
    m_micInit = 0;
    m_micStream = NULL;
	memset(&m_micParam, 0, sizeof(stAPParam));
    m_playInit = 0;
    m_playStream = NULL;
	memset(&m_playParam, 0, sizeof(stAPParam));
    m_play_data_list_max_cnt = 0;
    
    m_mic_is_play = 0;
}

AudioDevice::~AudioDevice()
{
    StopMic();
    StopPlay();
    UnInit();
}

int AudioDevice::Init()
{
    if(m_globalInit == 0)
    {
        PaError err = paNoError;
        err = Pa_Initialize();
        if (err != paNoError)
        {
            printf("Pa_Initialize err.\n");
            return -1;
        }
        
        m_globalInit = 1;
    }

    return 0;
}

int AudioDevice::UnInit()
{
    if(m_globalInit)
    {
		Pa_Terminate();
        m_globalInit = 0;
    }
    
    return 0;
}

int AudioDevice::StartMicDefaultDev()
{
    if(!m_globalInit)
    {
		return -1;
    }
    
    if(m_micInit)
    {
        printf("Mic already start.\n");
        return -1;
    }
    
    PaStreamParameters intputDefault;
    intputDefault.device = Pa_GetDefaultInputDevice(); /* default output device */
    if (intputDefault.device == paNoDevice)
    {
        printf("mic device err.\n");
        return -1;
    }
    
    stAPParam micParam = { 0 };
    micParam.chn = CHANNEL;
    micParam.formt = FORMAT;
    micParam.sampleRate = SAMPLE_RATE;
    micParam.framesPerBuffer = FRAMES_PER_BUFFER;
    micParam.devId = intputDefault.device;

	memcpy(&m_micParam, &micParam, sizeof(stAPParam));
    
    printf("StartMic device id:%d\n", m_micParam.devId);

    PaStreamParameters intputParameters;
    intputParameters.device = m_micParam.devId;
    intputParameters.channelCount = m_micParam.chn;
    if(m_micParam.formt == 8)
    {
        intputParameters.sampleFormat = paInt8;
    }
    else if(m_micParam.formt == 16)
    {
        intputParameters.sampleFormat = paInt16;
    }
    else
    {
        intputParameters.sampleFormat = paInt32;
    }
    intputParameters.suggestedLatency = Pa_GetDeviceInfo(intputParameters.device)->defaultLowInputLatency;
    intputParameters.hostApiSpecificStreamInfo = NULL;

    PaError err = paNoError;
    err = Pa_OpenStream((PaStream**)&m_micStream, &intputParameters, NULL, m_micParam.sampleRate, m_micParam.framesPerBuffer, paFramesPerBufferUnspecified, MicDataCallBack, this);
    if (err != paNoError)
    {
        printf("Pa_OpenStream err.\n");
        return -1;
    }
    err = Pa_StartStream((PaStream*)m_micStream);
    if (err != paNoError)
    {
        printf("Pa_StartStream err.\n");
        return -1;
    }
    
    m_micInit = 1;

    return 0;
}

int AudioDevice::StopMic()
{
    if(!m_micInit)
    {
        return 0;
    }
    
    if(m_micStream != NULL)
    {
        PaError err = paNoError;
        err = Pa_StopStream((PaStream*)m_micStream);
        if (err != paNoError)
        {
            printf("Pa_StopStream err.\n");
        }
        err = Pa_CloseStream((PaStream*)m_micStream);
        if (err != paNoError)
        {
            printf("Pa_CloseStream err.\n");
        }
        m_micStream = NULL;
    }
    
    m_mic_is_play = 0;
    m_micInit = 0;

    return 0;
}

int AudioDevice::GetMicDataAttach(CCPPObject* obj, CPPSIGAPI api)
{
	return m_micSig.Attach(obj, api);
}

int AudioDevice::GetMicDataDetach(CCPPObject* obj, CPPSIGAPI api)
{
	return m_micSig.Detach(obj, api);
}

void AudioDevice::AudioDataSend(const void* data)
{
    if(data == NULL)
    {
        return;
    }
    
    int len = m_micParam.framesPerBuffer * m_micParam.formt/8 * m_micParam.chn;
        
	CPacket* pAudioPkt = CPacket::Acquire(len+sizeof(FRAMEHEAD), PKG_SOUR_DevAudioIn);
	if(pAudioPkt != NULL)
	{
		/* 组装帧头结构体 */
		pAudioPkt->FrameInit();
		pAudioPkt->FrameSetCodec(CODEC_PCM);
		pAudioPkt->FrameSetType(PACK_TYPE_FRAME_AUDIO);
		pAudioPkt->FrameSetLength(len);
		pAudioPkt->FrameSetFreq(m_micParam.sampleRate);
		pAudioPkt->FrameSetChannum(m_micParam.chn);
		CCPPTime objTime;
		objTime.FillCurrentTime();
		pAudioPkt->FrameSetTime(objTime);
		memcpy(pAudioPkt->FrameGetData(), data, len);
        m_micSig.Commit(this, 1, (void*)pAudioPkt, NULL);

        if(m_mic_is_play)
        {
            PlayAudio(pAudioPkt);
        }
        
        pAudioPkt->Release();
	}

    return ;
}

int AudioDevice::StartPlayDefaultDev()
{
    if(!m_globalInit)
    {
		return -1;
    }
        
    CCPPGuard guard(m_playInit_mutex);
    
    if(m_playInit)
    {
        printf("Play already start.\n");
        return -1;
    }
    
    PaStreamParameters outputDefault;
    outputDefault.device = Pa_GetDefaultOutputDevice(); /* default output device */
    if (outputDefault.device == paNoDevice)
    {
        printf("Play device err.\n");
        return -1;
    }
        
    stAPParam playParam = { 0 };
    playParam.chn = CHANNEL;
    playParam.formt = FORMAT;
    playParam.sampleRate = SAMPLE_RATE;
    playParam.framesPerBuffer = FRAMES_PER_BUFFER;
    playParam.devId = outputDefault.device;

	memcpy(&m_playParam, &playParam, sizeof(stAPParam));
    
    printf("StartPlay device id:%d\n", m_playParam.devId);
    
    PaStreamParameters outputParameters;
    outputParameters.device = m_playParam.devId;
    outputParameters.channelCount = m_playParam.chn;
    if(m_playParam.formt == 8)
    {
        outputParameters.sampleFormat = paInt8;
    }
    else if(m_playParam.formt == 16)
    {
        outputParameters.sampleFormat = paInt16;
    }
    else
    {
        outputParameters.sampleFormat = paInt32;
    }
    outputParameters.suggestedLatency = Pa_GetDeviceInfo(outputParameters.device)->defaultLowOutputLatency;
    outputParameters.hostApiSpecificStreamInfo = NULL;
    
    PaError err = Pa_OpenStream((PaStream**)&m_playStream, NULL, &outputParameters, m_playParam.sampleRate, m_playParam.framesPerBuffer, paFramesPerBufferUnspecified, PlayDataCallback, this);
    if (err != paNoError)
    {
        printf("Pa_OpenStream err.\n");
        return -1;
    }
    
    err = Pa_StartStream((PaStream*)m_playStream);
    if (err != paNoError)
    {
        printf("Pa_StartStream err.\n");
        return -1;
    }
    
//  m_play_data_list_max_cnt = m_playParam.playSampleRate/m_playParam.playFramesPerBuffer*5;//最多缓存5s的数据
    m_playInit = 1;

    return 0;
}

int AudioDevice::StopPlay()
{
    CCPPGuard guard(m_playInit_mutex);
    
    if(!m_playInit)
    {
        return 0;
    }
    
    if(m_playStream != NULL)
    {
        PaError err = paNoError;
        err = Pa_StopStream((PaStream*)m_playStream);
        if (err != paNoError)
        {
            printf("Pa_StopStream err.\n");
        }
        err = Pa_CloseStream((PaStream*)m_playStream);
        if (err != paNoError)
        {
            printf("Pa_CloseStream err.\n");
        }
        m_playStream = NULL;
    }
    
    m_play_data_list_max_cnt = 0;
    m_play_data_list_mutex.Enter();
    m_play_data_list.Clear();
    m_play_data_list_mutex.Leave();
    
    m_playInit = 0;

    return 0;
}

int AudioDevice::PlayAudio(CPacket* pPacket)
{
    CCPPGuard guard(m_playInit_mutex);
    
    if(!m_playInit)
    {
		return -1;
    }
    
	if(pPacket == NULL)
	{
		return -1;
	}

    if(pPacket->FrameGetCodec() != CODEC_PCM)
    {
        return -1;
    }
    
    m_play_data_list_mutex.Enter();

    if(m_play_data_list_max_cnt != 0)
    {
        if(m_play_data_list.GetCount() < m_play_data_list_max_cnt)
        {
            pPacket->AddRef();
            int iRet = m_play_data_list.PutPacket(pPacket);
            if(iRet != CPP_OK)
            {
                pPacket->Release();
            }
        }
        else
        {
            printf("play audio data lose!!!!!!!!!!!!!!!!!!!!!!!!!\n");
        }
    }
    else
    {
        pPacket->AddRef();
        int iRet = m_play_data_list.PutPacket(pPacket);
        if(iRet != CPP_OK)
        {
            pPacket->Release();
        }
    }
    m_play_data_list_mutex.Leave();

    return CPP_OK;
}

int AudioDevice::PlayAudio(uchar* data, int len)
{
    if(data == NULL || len <= 0)
    {
        return CPP_FAIL;
    }
    
    int frames_len = FRAMES_PER_BUFFER*FORMAT/8;
    int processLen = 0;
    while(1)
    {
        if(processLen + frames_len >= len)
        {
            CPacket* pAudioPkt = CPacket::Acquire(frames_len+sizeof(FRAMEHEAD), PKG_SOUR_DevAudioIn);
            if(pAudioPkt != NULL)
            {
                /* 组装帧头结构体 */
                pAudioPkt->FrameInit();
                pAudioPkt->FrameSetCodec(CODEC_PCM);
                pAudioPkt->FrameSetType(PACK_TYPE_FRAME_AUDIO);
                pAudioPkt->FrameSetLength(frames_len);
                pAudioPkt->FrameSetFreq(SAMPLE_RATE);
                pAudioPkt->FrameSetChannum(CHANNEL);
                CCPPTime objTime;
                objTime.FillCurrentTime();
                pAudioPkt->FrameSetTime(objTime);
                memset(pAudioPkt->FrameGetData(), 0, pAudioPkt->FrameGetLength());
                memcpy(pAudioPkt->FrameGetData(), data + processLen, len - processLen);
                
                //播放
                PlayAudio(pAudioPkt);
                pAudioPkt->Release();
            }
            break;
        }
        else
        {
            CPacket* pAudioPkt = CPacket::Acquire(frames_len+sizeof(FRAMEHEAD), PKG_SOUR_DevAudioIn);
            if(pAudioPkt != NULL)
            {
                /* 组装帧头结构体 */
                pAudioPkt->FrameInit();
                pAudioPkt->FrameSetCodec(CODEC_PCM);
                pAudioPkt->FrameSetType(PACK_TYPE_FRAME_AUDIO);
                pAudioPkt->FrameSetLength(frames_len);
                pAudioPkt->FrameSetFreq(SAMPLE_RATE);
                pAudioPkt->FrameSetChannum(CHANNEL);
                CCPPTime objTime;
                objTime.FillCurrentTime();
                pAudioPkt->FrameSetTime(objTime);
                memcpy(pAudioPkt->FrameGetData(), data + processLen, frames_len);
                
                //播放
                PlayAudio(pAudioPkt);
                pAudioPkt->Release();
            }
            processLen += frames_len;
        }
    }

    return CPP_OK;
}

void AudioDevice::ClearAudio()
{
    m_play_data_list_mutex.Enter();
    m_play_data_list.Clear();
    m_play_data_list_mutex.Leave();
}

CPacket* AudioDevice::GetPlayAudio()
{
    m_play_data_list_mutex.Enter();
    CPacket* pPkt = m_play_data_list.GetPacket();
    m_play_data_list_mutex.Leave();
    return pPkt;
}

void AudioDevice::WaitPlayEnd()
{
    //不加锁，可能有隐患
    while(1)
    {
        if(m_play_data_list.IsEmpty())
        {
            break;
        }
        else
        {
            CPPSleepMS(100);
        }
    }

    return ;
}

