#ifndef __AUDIODEVICE_H__
#define __AUDIODEVICE_H__

#include "CPPBase.h"

#define SAMPLE_RATE 16000
#define FORMAT      16
#define CHANNEL     1
#define FRAMES_PER_BUFFER 1600

typedef struct stAPParam_t
{
    int chn;
    int sampleRate;
    int formt;
    int framesPerBuffer;
    int devId;
}stAPParam;

class AudioDevice : public CCPPObject
{
public:
    static AudioDevice* instance();

	AudioDevice();
    ~AudioDevice();

    int Init();
    int UnInit();

    int StartMicDefaultDev();
    int StopMic();
    
    //注册音频获取回调函数
	int GetMicDataAttach(CCPPObject* obj, CPPSIGAPI api);
	int GetMicDataDetach(CCPPObject* obj, CPPSIGAPI api);

	stAPParam* GetMicParam() { return &m_micParam; };

    void AudioDataSend(const void* data);
    
    //播放声音
	int StartPlayDefaultDev();
    int StopPlay();
    int PlayAudio(CPacket* pPacket);
    int PlayAudio(uchar* data, int len);
    void ClearAudio();

	stAPParam* GetPlayParam() { return &m_playParam; };
    
    CPacket* GetPlayAudio();
    void WaitPlayEnd();

private:
    int                 m_globalInit;
    
    int					m_micInit;
    void*               m_micStream;
    ICPPSignal			m_micSig;
	stAPParam		    m_micParam;
    
	CCPPMutex       	m_playInit_mutex;
    int                 m_playInit;
    void*               m_playStream;
	stAPParam		    m_playParam;
    int             	m_play_data_list_max_cnt;
    CCPPPacketList  	m_play_data_list;
	CCPPMutex       	m_play_data_list_mutex;

    //音频输入是否播放
    int                 m_mic_is_play;
};

#endif
