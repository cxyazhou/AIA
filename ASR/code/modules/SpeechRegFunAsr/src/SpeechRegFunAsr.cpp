#include <iostream>
#include <vector>
#include <fstream>

#include "SpeechRegFunAsr.h"
#include "FunAsrOnnxWrapper.h"
#include "TcpClient.h"

#define DATA_TIME   2
#define NAME_FRAME_NUM (SAMPLE_RATE/FRAMES_PER_BUFFER*2)
#define CMD_FRAME_NUM (SAMPLE_RATE/FRAMES_PER_BUFFER*5)

CPPSpeakRegFunAsr* CPPSpeakRegFunAsr::instance()
{
    static CPPSpeakRegFunAsr* _ins = NULL;
    if ( _ins == NULL )
    {
        _ins = new CPPSpeakRegFunAsr();
    }

    return _ins;
}

CPPSpeakRegFunAsr::CPPSpeakRegFunAsr()
{    
    m_init = 0;
    m_mic_audio_init = 0;
    m_play_audio_init = 1;
    m_thread_worker = NULL;
    m_data_list_max_cnt = 0;
    memset(&m_data, 0, sizeof(SR_AUDIO_DATA));
    memset(&m_name_work, 0, sizeof(SR_REG_NAME_WORK));
    memset(&m_cmd_work, 0, sizeof(SR_REG_CMD_WORK));
    m_name_reg_flag = 1;
    m_funasr = NULL;
    m_data_list_manual_speech_buffer_len = 0;
    m_wake_pcm_buffer = NULL;
    m_wake_pcm_buffer_size = 0;
    m_wake_pcm_buffer_len = 0;
    
    m_speak_sen = 0;
    m_start_dictation = 0;
    m_start_dictation_speaking = 0;
    m_dictation_speaking_pending = 0;
    m_dictation_speaking_pending_time = 0;
    m_dictation_data = NULL;
    m_dictation_data_size = 500*1024;//16s的数据
    m_dictation_data_reg_len_min = 16*1024;//至少识别半秒的数据
    m_dictation_data_len = 0;
    m_punc_enable = 0;
    m_media_is_playing = 0;
}

CPPSpeakRegFunAsr::~CPPSpeakRegFunAsr()
{
    Stop();
    UnInit();
}

int CPPSpeakRegFunAsr::Init()
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

            char path[1024] = { 0 };
            char processname[1024] = { 0 };
            ICPPFile::GetExecutablePath(path, processname, sizeof(path));

            m_funasr = new FunAsrOnnxWrapper;
            
            CCPPString asr_model(path);
            asr_model += "model/asr";
            
            int ret = m_funasr->Init(asr_model.GetBuffer(), "", 4, false);
            if (ret != 0)
            {
                std::cerr << "Failed to initialize ASR" << std::endl;
            }

            m_init = 1;
            
            return 0;
        }

        return -1;
    }
    
    return 0;
}

int CPPSpeakRegFunAsr::UnInit()
{
    Stop();
    
    if(m_init)
    {
        CPPDel(m_funasr);
                
        CPPFree(m_data.cur_buffer);
        CPPFree(m_data.remain_buffer);
        
        CPPFree(m_name_work.Buffer);
        CPPFree(m_name_work.Lastbuffer);
        
        CPPFree(m_cmd_work.Buffer);
        
        CPPFree(m_wake_pcm_buffer);

        m_init = 0;
    }
    return 0;
}

int CPPSpeakRegFunAsr::Start()
{    
    if (m_thread_worker != NULL)
    {
        m_thread_worker->Attach(this, (CPPTHREADAPI)&CPPSpeakRegFunAsr::ThreadProc);
    }
    
    return 0;
}

int CPPSpeakRegFunAsr::Stop()
{
    if (m_thread_worker != NULL)
    {
        m_thread_worker->Detach(this, (CPPTHREADAPI)&CPPSpeakRegFunAsr::ThreadProc);
        CPPDel(m_thread_worker);
    }
        
    return 0;
}

int CPPSpeakRegFunAsr::GetRstAttach(ICPPCallback* pCallback)
{
    if (pCallback == NULL)
        return CPPERR_HANDLE;
    return m_sig.Attach(pCallback);
}

int CPPSpeakRegFunAsr::GetRstDetach(ICPPCallback* pCallback)
{
    if (pCallback == NULL)
        return CPPERR_HANDLE;
    return m_sig.Detach(pCallback);
}

int CPPSpeakRegFunAsr::GetRstAttach(CCPPObject* obj, CPPSIGAPI api)
{
	return m_sig.Attach(obj, api);
}

int CPPSpeakRegFunAsr::GetRstDetach(CCPPObject* obj, CPPSIGAPI api)
{
	return m_sig.Detach(obj, api);
}

int CPPSpeakRegFunAsr::GetNameRstAttach(ICPPCallback* pCallback)
{
    if (pCallback == NULL)
        return CPPERR_HANDLE;
    return m_sig_name.Attach(pCallback);
}

int CPPSpeakRegFunAsr::GetNameRstDetach(ICPPCallback* pCallback)
{
    if (pCallback == NULL)
        return CPPERR_HANDLE;
    return m_sig_name.Detach(pCallback);
}

int CPPSpeakRegFunAsr::GetNameRstAttach(CCPPObject* obj, CPPSIGAPI api)
{
	return m_sig_name.Attach(obj, api);
}

int CPPSpeakRegFunAsr::GetNameRstDetach(CCPPObject* obj, CPPSIGAPI api)
{
	return m_sig_name.Detach(obj, api);
}

int CPPSpeakRegFunAsr::GetServerState()
{
    return 0;
}

int CPPSpeakRegFunAsr::OnGetXboxKey(CCPPObject* object, uint cmd, void* p0, void* p1)
{    
    return CPP_OK;
}

void CPPSpeakRegFunAsr::SetPuncEnable(int punc_enable)
{
    int reInit = 0;
    if(m_punc_enable != punc_enable)
    {
        reInit = 1;
    }
    
    m_punc_enable = punc_enable;

    if(!reInit)
    {
        return ;
    }
    
    m_funasr_mutex.Enter();
    if(m_funasr)
    {
        m_funasr->Uninit();
        
        char path[1024] = { 0 };
        char processname[1024] = { 0 };
        ICPPFile::GetExecutablePath(path, processname, sizeof(path));
        CCPPString asr_model(path);
        asr_model += "model/asr";

        CCPPString punc_model;
        if(punc_enable)
        {
            punc_model.Append("%s", path);
            punc_model += "model/punc";
        }

        const char* punc_ptr = punc_model.GetLength() ? punc_model.GetBuffer() : "";
        int ret = m_funasr->Init(asr_model.GetBuffer(), punc_ptr, 4, false);
        if (ret != 0)
        {
            std::cerr << "Failed to initialize ASR" << std::endl;
        }
    }
    m_funasr_mutex.Leave();
}

void CPPSpeakRegFunAsr::StartDictation()
{
    m_start_dictation = 1;
}

void CPPSpeakRegFunAsr::StopDictation()
{
    m_start_dictation = 0;
    CPPFree(m_dictation_data);
    m_dictation_data_len = 0;
}

void CPPSpeakRegFunAsr::StartDictationSpeak()
{
    m_dictation_speaking_pending = 1;
    m_dictation_speaking_pending_time = CCPPTime::GetClockTimeMS();
}

void CPPSpeakRegFunAsr::StopDictationSpeak()
{
    m_dictation_speaking_pending = 0;
    m_start_dictation_speaking = 0;
}

int CPPSpeakRegFunAsr::ManualSpeechRec(int speakFlag, CCPPString &rst)
{
    return 0;
}

int CPPSpeakRegFunAsr::AudioRec(const char* data, int len, CCPPString &rst)
{
    m_funasr_mutex.Enter();
    RecognitionResult result = m_funasr->RecognizeBuffer(data, len, 16000);
    int rstLen = strlen(result.text.c_str());
    char* final_rst = NULL;
    if(rstLen > 0)
    {
        final_rst = (char*)CPPMalloc0(rstLen+1);
        memcpy(final_rst, result.text.c_str(), rstLen);
    }
    m_funasr_mutex.Leave();
    std::cout << "Recognition Result:" << std::endl;
    CCPPString gbks;
    gbks.Init(10240);
    CCPPString::utf8ToGbk(result.text.c_str(), strlen(result.text.c_str()), gbks.GetBuffer(), 10240);
    std::cout << "Text: " << gbks.GetBuffer() << std::endl;
    std::cout << "Timestamp: " << result.timestamp << std::endl;
    std::cout << "Stamp Sents: " << result.stamp_sents << std::endl;
    std::cout << "Snippet Time: " << result.snippet_time << "s" << std::endl;
    if(final_rst != NULL)
    {
        rst.Append("%s", final_rst);
        CPPFree(final_rst);
    }
    else
    {
    }
    return 0;
}

int CPPSpeakRegFunAsr::LocalAudioRec(CCPPString &rst)
{
    m_funasr_mutex.Enter();
    RecognitionResult result = m_funasr->RecognizeBuffer(m_dictation_data, m_dictation_data_len, 16000);
    int rstLen = strlen(result.text.c_str());
    char* final_rst = NULL;
    if(rstLen > 0)
    {
        final_rst = (char*)CPPMalloc0(rstLen+1);
        memcpy(final_rst, result.text.c_str(), rstLen);
    }
    m_funasr_mutex.Leave();
    
    std::cout << "Recognition Result:" << std::endl;
    CCPPString gbks;
    gbks.Init(10240);
    CCPPString::utf8ToGbk(result.text.c_str(), strlen(result.text.c_str()), gbks.GetBuffer(), 10240);
    std::cout << "Text: " << gbks.GetBuffer() << std::endl;
    std::cout << "Timestamp: " << result.timestamp << std::endl;
    std::cout << "Stamp Sents: " << result.stamp_sents << std::endl;
    std::cout << "Snippet Time: " << result.snippet_time << "s" << std::endl;
                        
    if(final_rst != NULL)
    {
        rst.Append("%s", final_rst);
        CPPFree(final_rst);
    }
    else
    {
        //空命令
    }
    
    m_dictation_data_len = 0;

    return 0;
}

void CPPSpeakRegFunAsr::ThreadProc(CCPPMessageT* msg, uint timestamp, uint& timeout)
{
	timeout = 2000;
    
	return ;
}

int CPPSpeakRegFunAsr::OnMicData(CCPPObject* object, uint cmd, void* p0, void* p1)
{
    return CPP_OK;
}

int CPPSpeakRegFunAsr::OnMicDataManualSpeech(CCPPObject* object, uint cmd, void* p0, void* p1)
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

    if(m_play_audio_init)
    {
        //播放
//      AudioDevice::instance()->PlayAudio(pPkt);
    }
    
    pPkt->AddRef();
    int iRet = m_data_list_manual_speech.PutPacket(pPkt);
    if(iRet != CPP_OK)
    {
        pPkt->Release();
    }
    m_data_list_manual_speech_buffer_len += pPkt->FrameGetLength();

    return CPP_OK;
}

int CPPSpeakRegFunAsr::AudioShot(const void* input, unsigned long frameCount)
{
    return -1;
}

int CPPSpeakRegFunAsr::RegName(const void* input, unsigned long frameCount)
{
    return 0;
}

int CPPSpeakRegFunAsr::RegCmd(const void* input, unsigned long frameCount)
{
    return 0;
}
