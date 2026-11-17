/**
 * Copyright FunASR (https://github.com/alibaba-damo-academy/FunASR). All Rights Reserved.
 * MIT License  (https://opensource.org/licenses/MIT)
*/

#include "precomp.h"

namespace funasr {
CTTransformer::CTTransformer()
:env_(ORT_LOGGING_LEVEL_ERROR, ""),session_options{}
{
}

void CTTransformer::InitPunc(const std::string &punc_model, const std::string &punc_config, const std::string &token_file, int thread_num){
    session_options.SetIntraOpNumThreads(thread_num);
    session_options.SetGraphOptimizationLevel(ORT_ENABLE_ALL);
    session_options.DisableCpuMemArena();

    try{
        m_session = std::make_shared<Ort::Session>(env_, ORTSTRING(punc_model).c_str(), session_options);
        LOG(INFO) << "Successfully load model from " << punc_model;
    }
    catch (std::exception const &e) {
        LOG(ERROR) << "Error when load punc onnx model: " << e.what();
        exit(-1);
    }
    // read inputnames outputnames
    GetInputNames(m_session.get(), m_strInputNames, m_szInputNames);
    GetOutputNames(m_session.get(), m_strOutputNames, m_szOutputNames);

    LOG(INFO) << "Punc model input count: " << m_szInputNames.size();
    for (size_t i = 0; i < m_szInputNames.size(); ++i) {
        LOG(INFO) << "  Punc input[" << i << "]: " << m_szInputNames[i];
    }
    LOG(INFO) << "Punc model output count: " << m_szOutputNames.size();
    for (size_t i = 0; i < m_szOutputNames.size(); ++i) {
        LOG(INFO) << "  Punc output[" << i << "]: " << m_szOutputNames[i];
    }
    
	m_tokenizer.OpenYaml(punc_config.c_str(), token_file.c_str());
    m_tokenizer.JiebaInit(punc_config);
}

CTTransformer::~CTTransformer()
{
}

string CTTransformer::AddPunc(const char* sz_input, std::string language)
{
    string strResult;

    // 检查输入是否有效
    if (sz_input == NULL || strlen(sz_input) == 0) {
        LOG(WARNING) << "CTTransformer::AddPunc: empty input";
        return strResult;
    }

    vector<string> strOut;
    vector<int> InputData;
    m_tokenizer.Tokenize(sz_input, strOut, InputData); 

    // 检查 tokenize 结果
    if (InputData.empty()) {
        LOG(WARNING) << "CTTransformer::AddPunc: tokenize returned empty";
        return strResult;
    }

    int nTotalBatch = ceil((float)InputData.size() / TOKEN_LEN);
    int nCurBatch = -1;
    int nSentEnd = -1, nLastCommaIndex = -1;
    vector<int32_t> RemainIDs; // 
    vector<string> RemainStr; //
    vector<int> NewPunctuation; //
    vector<string> NewString; //
    vector<string> NewSentenceOut;
    vector<int> NewPuncOut;
    int nDiff = 0;
    for (size_t i = 0; i < InputData.size(); i += TOKEN_LEN)
    {
        nDiff = (i + TOKEN_LEN) < InputData.size() ? (0) : (i + TOKEN_LEN - InputData.size());
        vector<int32_t> InputIDs(InputData.begin() + i, InputData.begin() + i + (TOKEN_LEN - nDiff));
        vector<string> InputStr(strOut.begin() + i, strOut.begin() + i + (TOKEN_LEN - nDiff));
        InputIDs.insert(InputIDs.begin(), RemainIDs.begin(), RemainIDs.end()); // RemainIDs+InputIDs;
        InputStr.insert(InputStr.begin(), RemainStr.begin(), RemainStr.end()); // RemainStr+InputStr;

        auto Punction = Infer(InputIDs);
        nCurBatch = i / TOKEN_LEN;
        if (nCurBatch < nTotalBatch - 1) // not the last minisetence
        {
            nSentEnd = -1;
            nLastCommaIndex = -1;
            for (int nIndex = Punction.size() - 2; nIndex > 0; nIndex--)
            {
                if (m_tokenizer.Id2Punc(Punction[nIndex]) == m_tokenizer.Id2Punc(PERIOD_INDEX) || m_tokenizer.Id2Punc(Punction[nIndex]) == m_tokenizer.Id2Punc(QUESTION_INDEX))
                {
                    nSentEnd = nIndex;
                    break;
                }
                if (nLastCommaIndex < 0 && m_tokenizer.Id2Punc(Punction[nIndex]) == m_tokenizer.Id2Punc(COMMA_INDEX))
                {
                    nLastCommaIndex = nIndex;
                }
            }
            if (nSentEnd < 0 && InputStr.size() > CACHE_POP_TRIGGER_LIMIT && nLastCommaIndex > 0)
            {
                nSentEnd = nLastCommaIndex;
                Punction[nSentEnd] = PERIOD_INDEX;
            }
            RemainStr.assign(InputStr.begin() + (nSentEnd + 1), InputStr.end());
            RemainIDs.assign(InputIDs.begin() + (nSentEnd + 1), InputIDs.end());
            InputStr.assign(InputStr.begin(), InputStr.begin() + (nSentEnd + 1));  // minit_sentence
            Punction.assign(Punction.begin(), Punction.begin() + (nSentEnd + 1));
        }
        
        NewPunctuation.insert(NewPunctuation.end(), Punction.begin(), Punction.end());
        vector<string> WordWithPunc;
        for (int i = 0; i < InputStr.size(); i++)
        {
            // if (i > 0 && !(InputStr[i][0] & 0x80) && (i + 1) <InputStr.size() && !(InputStr[i+1][0] & 0x80))// �м��Ӣ�ģ�
            if (i > 0 && !(InputStr[i-1][0] & 0x80) && !(InputStr[i][0] & 0x80))
            {
                InputStr[i] = " " + InputStr[i];
            }
            WordWithPunc.push_back(InputStr[i]);

            if (Punction[i] != NOTPUNC_INDEX) // �»���
            {
                WordWithPunc.push_back(m_tokenizer.Id2Punc(Punction[i]));
            }
        }

        NewString.insert(NewString.end(), WordWithPunc.begin(), WordWithPunc.end()); // new_mini_sentence += "".join(words_with_punc)
        NewSentenceOut = NewString;
        NewPuncOut = NewPunctuation;
        // last mini sentence
        if(nCurBatch == nTotalBatch - 1)
        {
            if (NewString[NewString.size() - 1] == m_tokenizer.Id2Punc(COMMA_INDEX) || NewString[NewString.size() - 1] == m_tokenizer.Id2Punc(DUN_INDEX))
            {
                NewSentenceOut.assign(NewString.begin(), NewString.end() - 1);
                NewSentenceOut.push_back(m_tokenizer.Id2Punc(PERIOD_INDEX));
                NewPuncOut.assign(NewPunctuation.begin(), NewPunctuation.end() - 1);
                NewPuncOut.push_back(PERIOD_INDEX);
            }
            else if (NewString[NewString.size() - 1] != m_tokenizer.Id2Punc(PERIOD_INDEX) && NewString[NewString.size() - 1] != m_tokenizer.Id2Punc(QUESTION_INDEX))
            {
                NewSentenceOut = NewString;
                NewSentenceOut.push_back(m_tokenizer.Id2Punc(PERIOD_INDEX));
                NewPuncOut = NewPunctuation;
                NewPuncOut.push_back(PERIOD_INDEX);
            }
        }
    }

    for (auto& item : NewSentenceOut){
        strResult += item;
    }
    
    if(language == "en-bpe"){
        std::vector<std::string> chineseSymbols;
        chineseSymbols.push_back("，");
        chineseSymbols.push_back("。");
        chineseSymbols.push_back("、");
        chineseSymbols.push_back("？");

        std::string englishSymbols = ",.,?";
        for (size_t i = 0; i < chineseSymbols.size(); i++) {
            size_t pos = 0;
            while ((pos = strResult.find(chineseSymbols[i], pos)) != std::string::npos) {
                strResult.replace(pos, 3, 1, englishSymbols[i]);
                pos++;
            }
        }
    }

    return strResult;
}

vector<int> CTTransformer::Infer(vector<int32_t> input_data)
{
    vector<int> punction;

    if (input_data.empty() || !m_session) {
        LOG(WARNING) << "CTTransformer::Infer: empty input or session not initialized";
        return punction;
    }

    if (m_szInputNames.empty() || m_szOutputNames.empty()) {
        LOG(WARNING) << "CTTransformer::Infer: input/output names not initialized";
        return punction;
    }

    Ort::MemoryInfo m_memoryInfo = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
    int64_t input_len = (int64_t)input_data.size();

    // 根据输入名称动态构建所有输入张量（支持4输入：inputs + text_lengths + vad_masks + sub_masks）
    std::vector<Ort::Value> input_onnx;
    std::vector<std::vector<int32_t>> keep_int_vecs;
    std::vector<std::vector<float>> keep_float_vecs;
    std::vector<std::array<int64_t, 2>> keep_shape2;
    std::vector<std::array<int64_t, 4>> keep_shape4;
    std::vector<std::array<int64_t, 1>> keep_shape1;

    for (size_t i = 0; i < m_szInputNames.size(); ++i) {
        std::string name = m_szInputNames[i];
        bool is_length = (name.find("length") != std::string::npos);
        bool is_mask = (name.find("mask") != std::string::npos || name.find("Mask") != std::string::npos);

        if (is_length) {
            // text_lengths: int32, shape [1]
            keep_shape1.push_back(std::array<int64_t, 1>{1});
            keep_int_vecs.push_back(std::vector<int32_t>{(int32_t)input_len});
            Ort::Value onnx = Ort::Value::CreateTensor<int32_t>(
                m_memoryInfo,
                keep_int_vecs.back().data(),
                keep_int_vecs.back().size(),
                keep_shape1.back().data(),
                keep_shape1.back().size());
            input_onnx.emplace_back(std::move(onnx));
        } else if (is_mask) {
            // vad_masks / sub_masks: float32, shape [1, 1, input_len, input_len]
            keep_shape4.push_back(std::array<int64_t, 4>{1, 1, input_len, input_len});
            keep_float_vecs.push_back(std::vector<float>((size_t)(input_len * input_len), 0.0f));
            Ort::Value onnx = Ort::Value::CreateTensor<float>(
                m_memoryInfo,
                keep_float_vecs.back().data(),
                keep_float_vecs.back().size(),
                keep_shape4.back().data(),
                keep_shape4.back().size());
            input_onnx.emplace_back(std::move(onnx));
        } else {
            // inputs: int32, shape [1, input_len]
            keep_shape2.push_back(std::array<int64_t, 2>{1, input_len});
            Ort::Value onnx = Ort::Value::CreateTensor<int32_t>(
                m_memoryInfo,
                input_data.data(),
                input_data.size(),
                keep_shape2.back().data(),
                keep_shape2.back().size());
            input_onnx.emplace_back(std::move(onnx));
        }
    }
    
    try {
        auto outputTensor = m_session->Run(Ort::RunOptions{nullptr}, m_szInputNames.data(), input_onnx.data(), input_onnx.size(), m_szOutputNames.data(), m_szOutputNames.size());
        std::vector<int64_t> outputShape = outputTensor[0].GetTensorTypeAndShapeInfo().GetShape();

        int64_t outputCount = std::accumulate(outputShape.begin(), outputShape.end(), 1, std::multiplies<int64_t>());
        float * floatData = outputTensor[0].GetTensorMutableData<float>();

        for (int i = 0; i < outputCount; i += CANDIDATE_NUM)
        {
            int index = Argmax(floatData + i, floatData + i + CANDIDATE_NUM-1);
            punction.push_back(index);
        }
    }
    catch (std::exception const &e)
    {
        LOG(ERROR) << "Error when run punc onnx forword: " << (e.what());
    }
    return punction;
}

} // namespace funasr
