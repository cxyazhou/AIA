#ifndef ESPEAK_H
#define ESPEAK_H

#include <vector>
#include "CPPBase.h"

class Espeak {
public:
    static Espeak* instance();
    
    Espeak();
    ~Espeak();

    // 初始化 espeak-ng
    // dataPath: espeak-ng-data 目录所在路径，传 NULL 则使用默认位置
    // voice: 语音名称，如 "en"、"zh"、"cmn" 等，传 NULL 则使用默认
    // 返回: true 成功，false 失败
    bool init(const char* dataPath = nullptr, const char* voice = "en");

    // 合成文本为 PCM 数据（16000Hz, 16-bit, 单声道）
    // text: 输入文本
    // pcm:  输出 PCM 首地址，内部用 malloc 申请，外部需调用 free() 释放
    // len:  输出样本数量（每个样本 1 个 short = 2 字节）
    // 返回: true 成功，false 失败
    bool GetAudio(const char* text, char** pcm, int* len);

    // 释放资源
    void uninit();

private:
    int resampleTo16k(const short* src, int srcCount, int srcRate, short** dst);
    
private:
    bool               m_initialized;
	CCPPMutex          m_mutex;
    
public:
    std::vector<short> m_pcmBuffer;
};

#endif // ESPEAK_H
