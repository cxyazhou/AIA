#include <iostream>
#include <vector>
#include <fstream>

#include "PlayAudio.h"
#include "AudioDevice.h"

CPPPlayAudio* CPPPlayAudio::instance()
{
    static CPPPlayAudio* _ins = NULL;
    if ( _ins == NULL )
    {
        _ins = new CPPPlayAudio();
    }

    return _ins;
}

CPPPlayAudio::CPPPlayAudio()
{    
    m_init = 0;
    m_mic_audio_init = 0;
    m_thread_worker = NULL;
    m_data_list_max_cnt = 0;

    m_start_dictation = 0;
    m_start_dictation_speaking = 0;
    m_dictation_speaking_pending = 0;
    m_dictation_speaking_pending_time = 0;
    m_dictation_data = NULL;
    m_dictation_data_size = 500*1024;//16s的数据
    m_dictation_data_reg_len_min = 16*1024;//至少识别半秒的数据
    m_dictation_data_len = 0;
}

CPPPlayAudio::~CPPPlayAudio()
{
    Stop();
    UnInit();
}

int CPPPlayAudio::Init()
{
    if(m_init == 0)
    {
        while(1)
        {            
            m_thread_worker = new CCPPThreadWorker(2, "SpeakReg");
            if(m_thread_worker == NULL)
            {
                break;
            }
            
            m_init = 1;
            
            return 0;
        }

        return -1;
    }
    
    return 0;
}

int CPPPlayAudio::UnInit()
{
    Stop();
    
    if(m_init)
    {
        m_init = 0;
    }
    return 0;
}

int CPPPlayAudio::Start()
{    
    AudioDevice::instance()->GetMicDataAttach(this, (CPPSIGAPI)&CPPPlayAudio::OnMicData);
    
    if (m_thread_worker != NULL)
    {
        m_thread_worker->Attach(this, (CPPTHREADAPI)&CPPPlayAudio::ThreadProc);
    }
    
    return 0;
}

int CPPPlayAudio::Stop()
{
    AudioDevice::instance()->GetMicDataDetach(this, (CPPSIGAPI)&CPPPlayAudio::OnMicData);
    
    if (m_thread_worker != NULL)
    {
        m_thread_worker->Detach(this, (CPPTHREADAPI)&CPPPlayAudio::ThreadProc);
        CPPDel(m_thread_worker);
    }
        
    return 0;
}

void CPPPlayAudio::StartSpeak()
{
    m_dictation_speaking_pending = 1;
    m_dictation_speaking_pending_time = CCPPTime::GetClockTimeMS();
}

void CPPPlayAudio::StopSpeak()
{
    m_dictation_speaking_pending = 0;
    m_start_dictation_speaking = 0;
}

void CPPPlayAudio::ThreadProc(CCPPMessageT* msg, uint timestamp, uint& timeout)
{
	timeout = 20;

    while(1)
    {
        m_data_list_mutex.Enter();
        CPacket* pPkt = m_data_list.GetPacket();
        m_data_list_mutex.Leave();
        if(pPkt == NULL)
        {
            break ;
        }
        
        int dataLen = pPkt->FrameGetLength();
        uchar* data = pPkt->FrameGetData();

        if(m_dictation_speaking_pending)
        {
            if((CCPPTime::GetClockTimeMS() - m_dictation_speaking_pending_time) >= 200)
            {
                m_dictation_speaking_pending = 0;
                m_start_dictation_speaking = 1;
            }
        }
        
        if(m_start_dictation_speaking)
        {
            if (m_dictation_data_len == 0)
            {                    
                AudioDevice::instance()->ClearAudio();
            }
            if(m_dictation_data == NULL)
            {
                m_dictation_data = (char*)malloc(m_dictation_data_size);
            }
            if(m_dictation_data_len + dataLen <= m_dictation_data_size)
            {
                memcpy(m_dictation_data + m_dictation_data_len, data, dataLen);
                m_dictation_data_len += dataLen;
            }
        }
        else
        {
            if(m_dictation_data_len)
            {
                if(m_dictation_data_len >= m_dictation_data_reg_len_min)
                {
                    printf("send audio\n");
                    TcpClientSendRecordAudio("pcm", NULL, (unsigned char*)m_dictation_data, m_dictation_data_len);
                }
                m_dictation_data_len = 0;
            }
        }
            
        pPkt->Release();
    }
    
	return ;
}

int CPPPlayAudio::OnMicData(CCPPObject* object, uint cmd, void* p0, void* p1)
{
	CPacket* pPkt = (CPacket*)p0;
	if(pPkt == NULL)
	{
		return CPP_FAIL;
	}

    if(pPkt->FrameGetCodec() != CODEC_PCM)
    {
        return CPP_FAIL;
    }

    //根据长度来计算缓存个数
    if(m_data_list_max_cnt == 0)
    {
        int frameLen = pPkt->FrameGetLength();
        int oneSecondLen = SAMPLE_RATE*(FORMAT/8)*CHANNEL;
        m_data_list_max_cnt = oneSecondLen/frameLen;//最多缓存1s的数据
    }
    
    m_data_list_mutex.Enter();
    
    if(m_data_list.GetCount() < m_data_list_max_cnt)
    {
        pPkt->AddRef();
        int iRet = m_data_list.PutPacket(pPkt);
        if(iRet != CPP_OK)
        {
            pPkt->Release();
        }
    }
    else
    {
        printf("audio data lose!!!!!!!!!!!!!!!!!!!!!!!!!\n");
    }
    m_data_list_mutex.Leave();

    return CPP_OK;
}
