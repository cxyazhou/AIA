#include "FunAsrOnnxWrapper.h"

#include <map>
#include <iostream>
#include <fstream>
#include <sstream>

// 直接使用 FunASR Runtime 的 API
#include "funasrruntime.h"

FunAsrOnnxWrapper::FunAsrOnnxWrapper()
    : m_asr_handle(nullptr)
    , m_initialized(false)
{
}

FunAsrOnnxWrapper::~FunAsrOnnxWrapper()
{
    Uninit();
}

int FunAsrOnnxWrapper::Init(const std::string& model_dir,
    const std::string& punc_dir,
    int thread_num,
    bool use_gpu)
{
    if (m_initialized)
    {
        return 0;
    }

    //转为Unicode
    CCPPString gbks;
    gbks.Init(10240);
    CCPPString::utf8ToGbk(model_dir.c_str(), strlen(model_dir.c_str()), gbks.GetBuffer(), 10240);
    
    std::map<std::string, std::string> model_path;
    model_path["model-dir"] = gbks.GetBuffer();

    if (!punc_dir.empty())
    {
        gbks.Empty();
        CCPPString::utf8ToGbk(punc_dir.c_str(), strlen(punc_dir.c_str()), gbks.GetBuffer(), 10240);
        
        model_path["punc-dir"] = gbks.GetBuffer();
    }

    model_path["QUANTIZE"] = "false";
    model_path["BLADEDISC"] = "false";

    // 使用 FunOfflineInit 初始化离线 ASR 模型
    m_asr_handle_mutex.Enter();
    m_asr_handle = FunOfflineInit(model_path, thread_num, use_gpu, 1);
    m_asr_handle_mutex.Leave();
    if (!m_asr_handle)
    {
        std::cerr << "Failed to initialize ASR model" << std::endl;
        return -1;
    }

    m_initialized = true;
    
    return 0;
}

RecognitionResult FunAsrOnnxWrapper::RecognizeBuffer(const char* audio_data,
    int data_len,
    int sampling_rate,
    const std::vector<std::string>& hotwords)
{
    RecognitionResult result;
    result.text = "";
    result.timestamp = "[]";
    result.stamp_sents = "[]";
    result.snippet_time = 0.0f;
    const char* stamp = NULL;
    FUNASR_RESULT rst = NULL;

    if (!m_initialized || !m_asr_handle || !audio_data || data_len <= 0)
    {
        std::cerr << "Invalid parameters or not initialized" << std::endl;
        return result;
    }

    try
    {
        std::vector<std::vector<float>> hotwords_embedding;

        // 使用 FunOfflineInferBuffer 进行缓冲区识别
        m_asr_handle_mutex.Enter();
        rst = FunOfflineInferBuffer(m_asr_handle, audio_data, data_len, RASRM_CTC_GREEDY_SEARCH, nullptr, hotwords_embedding, sampling_rate, "pcm", true);
        m_asr_handle_mutex.Leave();

        if (rst)
        {
            // 获取识别结果文本
            const char* text = FunASRGetResult(rst, 0);
            if (text)
            {
                result.text = std::string(text);
            }

            // 获取词级时间戳
            stamp = FunASRGetStamp(rst);
            if (stamp)
            {
                result.timestamp = std::string(stamp);
            }

            // 获取句子级时间戳
            const char* stamp_sents = FunASRGetStampSents(rst);
            if (stamp_sents)
            {
                result.stamp_sents = std::string(stamp_sents);
            }

            // 获取音频时长
            result.snippet_time = FunASRGetRetSnippetTime(rst);

            // 释放结果
            FunASRFreeResult(rst);
        }
    }
    catch (const std::exception& e)
    {
        std::cerr << "Recognition error: " << e.what() << std::endl;
    }

    return result;
}

void FunAsrOnnxWrapper::Uninit()
{
    if (!m_initialized)
    {
        return;
    }

    try
    {
        // 使用 FunASRUninit 释放 ASR 模型
        m_asr_handle_mutex.Enter();
        if (m_asr_handle)
        {
            FunOfflineUninit(m_asr_handle);
            m_asr_handle = nullptr;
        }
        m_asr_handle_mutex.Leave();

        m_initialized = false;
    }
    catch (const std::exception& e)
    {
        std::cerr << "Uninit error: " << e.what() << std::endl;
    }
}

