#ifndef __SPEECH_REG_FUNASR_H__
#define __SPEECH_REG_FUNASR_H__

#include <iostream>
#include <stdio.h>

#include "CPPBase.h"

using namespace std;

typedef struct tagSR_AUDIO_DATA
{
    int   sample_rate;

    char* cur_buffer;
    int   cur_buffer_size;
    int   cur_buffer_len;
    
    char* remain_buffer;
    int   remain_buffer_size;
    int   remain_buffer_len;
} SR_AUDIO_DATA;

typedef struct tagSR_REG_NAME_WORK
{
    int     SpeakStart;
    int     FrameSize;
    int     BufferSize;
    char*   Buffer;
    char*   Lastbuffer;
    int     Len;
    int     InitCnt;
} SR_REG_NAME_WORK;

typedef struct tagSR_REG_CMD_WORK
{
    int     SpeakStart;
    int     FrameSize;
    int     BufferSize;
    char*   Buffer;
    int     Len;
} SR_REG_CMD_WORK;

class FunASRClient;
class FunAsrOnnxWrapper;
class CPPSpeakRegFunAsr : public CCPPObject
{
public:
    static CPPSpeakRegFunAsr* instance();
    
    CPPSpeakRegFunAsr();
    ~CPPSpeakRegFunAsr();
    
    int Init();
    int UnInit();
    
	int Start();
    int Stop();
    
    int GetNameRstAttach(ICPPCallback* pCallback);
    int GetNameRstDetach(ICPPCallback* pCallback);

    int GetNameRstAttach(CCPPObject* obj, CPPSIGAPI api);
    int GetNameRstDetach(CCPPObject* obj, CPPSIGAPI api);

    int GetRstAttach(ICPPCallback* pCallback);
    int GetRstDetach(ICPPCallback* pCallback);

    int GetRstAttach(CCPPObject* obj, CPPSIGAPI api);
	int GetRstDetach(CCPPObject* obj, CPPSIGAPI api);

    int GetServerState();
    int GetSpeakSen() { return m_speak_sen; };

    int BeforeWakeMediaIsPlaying() { return m_media_is_playing; };
    
    int OnGetXboxKey(CCPPObject* object, uint cmd, void* p0, void* p1);

    void SetPuncEnable(int punc_enable);
    int  GetPuncEnable() { return m_punc_enable; };
    void StartDictation();
    void StopDictation();
    void StartDictationSpeak();
    void StopDictationSpeak();
    
    int ManualSpeechRec(int speakFlag, CCPPString &rst);
    int AudioRec(const char* data, int len, CCPPString &rst);
    
    int LocalAudioRec(CCPPString &rst);
	
private:
    void ThreadProc(CCPPMessageT* msg, uint timestamp, uint& timeout);
    int  OnMicData(CCPPObject* object, uint cmd, void* p0, void* p1);
    int  OnMicDataManualSpeech(CCPPObject* object, uint cmd, void* p0, void* p1);

    int  AudioShot(const void* input, unsigned long frameCount);
    int  RegName(const void* input, unsigned long frameCount);
    int  RegCmd(const void* input, unsigned long frameCount);

protected:
    int             m_data_list_max_cnt;
    CCPPPacketList  m_data_list;
	CCPPMutex       m_data_list_mutex;
    
    CCPPPacketList  m_data_list_manual_speech;
    int             m_data_list_manual_speech_buffer_len;
    
    SR_AUDIO_DATA   m_data;
    SR_REG_NAME_WORK m_name_work;
    int             m_name_reg_flag;
    
    SR_REG_CMD_WORK m_cmd_work;

private:
    int                 m_init;
    int                 m_mic_audio_init;
    int                 m_play_audio_init;
    ICPPSignal          m_sig;
    ICPPSignal          m_sig_name;
    CCPPThreadWorker*   m_thread_worker;

    int                 m_speak_sen;

    FunAsrOnnxWrapper*  m_funasr;
    CCPPMutex           m_funasr_mutex;

    int                 m_start_dictation;
    int                 m_start_dictation_speaking;
    int                 m_dictation_speaking_pending;
    uint                m_dictation_speaking_pending_time;
    char*               m_dictation_data;
    int                 m_dictation_data_size;
    int                 m_dictation_data_len;
    int                 m_dictation_data_reg_len_min;
    int                 m_punc_enable;
    
    int                 m_media_is_playing;

public:    
    char*               m_wake_pcm_buffer;
    int                 m_wake_pcm_buffer_size;
    int                 m_wake_pcm_buffer_len;
};

#endif //__SPEECH_REG_FUNASR_H__
