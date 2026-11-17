#ifndef __RECORD_AUDIO_H__
#define __RECORD_AUDIO_H__

#include <iostream>
#include <stdio.h>

#include "CPPBase.h"

using namespace std;

typedef struct tagRECORD_AUDIO_DATA
{
    int   sample_rate;

    char* cur_buffer;
    int   cur_buffer_size;
    int   cur_buffer_len;
    
    char* remain_buffer;
    int   remain_buffer_size;
    int   remain_buffer_len;
} RECORD_AUDIO_DATA;

class CPPPlayAudio : public CCPPObject
{
public:
    static CPPPlayAudio* instance();
    
    CPPPlayAudio();
    ~CPPPlayAudio();
    
    int Init();
    int UnInit();
    
	int Start();
    int Stop();
    
    void StartSpeak();
    void StopSpeak();
    
private:
    void ThreadProc(CCPPMessageT* msg, uint timestamp, uint& timeout);
    int  OnMicData(CCPPObject* object, uint cmd, void* p0, void* p1);

protected:
    int             m_data_list_max_cnt;
    CCPPPacketList  m_data_list;
	CCPPMutex       m_data_list_mutex;
        
private:
    int                 m_init;
    int                 m_mic_audio_init;
    CCPPThreadWorker*   m_thread_worker;

    int                 m_start_dictation;
    int                 m_start_dictation_speaking;
    int                 m_dictation_speaking_pending;
    uint                m_dictation_speaking_pending_time;
    char*               m_dictation_data;
    int                 m_dictation_data_size;
    int                 m_dictation_data_len;
    int                 m_dictation_data_reg_len_min;
};

#endif //__RECORD_AUDIO_H__
