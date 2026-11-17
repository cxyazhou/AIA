#ifndef FUNASR_ONNX_WRAPPER_H
#define FUNASR_ONNX_WRAPPER_H

#include <iostream>
#include <string>
#include <vector>

#include "CPPBase.h"

#define FUNASR_API

struct RecognitionResult
{
    std::string text;
    std::string timestamp;
    std::string stamp_sents;
    float snippet_time;
};

class FUNASR_API FunAsrOnnxWrapper
{
public:
    FunAsrOnnxWrapper();
    ~FunAsrOnnxWrapper();

    int Init(const std::string& model_dir, 
             const std::string& punc_dir = "",
             int thread_num = 4,
             bool use_gpu = false);

    RecognitionResult RecognizeBuffer(const char* audio_data,
                                       int data_len,
                                       int sampling_rate = 16000,
                                       const std::vector<std::string>& hotwords = {});

    void Uninit();

private:
    CCPPMutex m_asr_handle_mutex;
    void* m_asr_handle;
    bool m_initialized;
};

#endif
