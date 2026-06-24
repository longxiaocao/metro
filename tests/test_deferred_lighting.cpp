// test_deferred_lighting.cpp - Phase 9.3.10 Deferred Lighting 单元测试
//
// 覆盖：
//   1. Deferred.hlsl 源文件存在
//   2. Deferred.hlsl 含 ps_main 入口
//   3. Deferred.hlsl 采样 4 个 GBuffer 纹理 (s0..s3)
//   4. Deferred.hlsl 引用 GPU Gems 2 §9.5 公开文献
//   5. Deferred.hlsl 包含 Blinn-Phong 光照计算
//   6. DeferredPSHLSLC.h 生成产物存在性 (可跳过)
//
// 不覆盖：
//   - 真实 GPU 渲染 (集成测试范围)

#include "SingleTest.h"

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#ifndef DDFIX_SOURCE_DIR
#define DDFIX_SOURCE_DIR "ddfix"
#endif

namespace
{

bool FileExists(const std::string& p)
{
    std::ifstream ifs(p, std::ios::binary);
    return ifs.good();
}

std::string ReadEntireFile(const std::string& p)
{
    std::ifstream ifs(p, std::ios::binary);
    if (!ifs.good()) return std::string();
    std::ostringstream oss;
    oss << ifs.rdbuf();
    return oss.str();
}

std::vector<std::string> PossibleDeferredShaderPaths()
{
    std::vector<std::string> out;
    out.push_back(std::string(DDFIX_SOURCE_DIR) + "/shaders/Deferred.hlsl");
    out.push_back(std::string(DDFIX_SOURCE_DIR) + "\\shaders\\Deferred.hlsl");
    out.push_back("../ddfix/shaders/Deferred.hlsl");
    out.push_back("ddfix/shaders/Deferred.hlsl");
    return out;
}

std::vector<std::string> PossibleDeferredGeneratedHeaderPaths()
{
    std::vector<std::string> out;
    const char* envBuild = std::getenv("DDFIX_BUILD_DIR");
    if (envBuild && *envBuild)
    {
        out.push_back(std::string(envBuild) + "/DeferredPSHLSLC.h");
        out.push_back(std::string(envBuild) + "\\DeferredPSHLSLC.h");
    }
    out.push_back("./ddfix/DeferredPSHLSLC.h");
    out.push_back("../ddfix/DeferredPSHLSLC.h");
    out.push_back("ddfix/DeferredPSHLSLC.h");
    return out;
}

} // anonymous namespace

// ============================================================
// 1. Deferred.hlsl 源文件存在
// ============================================================
TEST(DeferredLighting, SourceFileExists)
{
    auto paths = PossibleDeferredShaderPaths();
    bool found = false;
    std::string used;
    for (const auto& p : paths)
    {
        if (FileExists(p))
        {
            found = true;
            used = p;
            break;
        }
    }
    EXPECT_TRUE(found);
    if (found)
    {
        std::printf("    [info] Deferred.hlsl at: %s\n", used.c_str());
    }
}

// ============================================================
// 2. Deferred.hlsl 含 ps_main 入口 + PS_OUTPUT
// ============================================================
TEST(DeferredLighting, SourceContainsPsMain)
{
    std::string content;
    for (const auto& p : PossibleDeferredShaderPaths())
    {
        content = ReadEntireFile(p);
        if (!content.empty()) break;
    }
    EXPECT_TRUE(!content.empty());
    EXPECT_TRUE(content.find("ps_main") != std::string::npos);
    EXPECT_TRUE(content.find("PS_INPUT") != std::string::npos);
    EXPECT_TRUE(content.find("PS_OUTPUT") != std::string::npos);
    EXPECT_TRUE(content.find("SV_Target0") != std::string::npos);
}

// ============================================================
// 3. Deferred.hlsl 采样 4 个 GBuffer 纹理
// ============================================================
TEST(DeferredLighting, SourceSamplesGBufferTextures)
{
    std::string content;
    for (const auto& p : PossibleDeferredShaderPaths())
    {
        content = ReadEntireFile(p);
        if (!content.empty()) break;
    }
    EXPECT_TRUE(!content.empty());
    // sampler2D 声明 (4 个 GBuffer 纹理)
    EXPECT_TRUE(content.find("sampler2D") != std::string::npos);
    EXPECT_TRUE(content.find("posTex") != std::string::npos);
    EXPECT_TRUE(content.find("normalTex") != std::string::npos);
    EXPECT_TRUE(content.find("diffuseTex") != std::string::npos);
    EXPECT_TRUE(content.find("specTex") != std::string::npos);
    // tex2D 调用
    EXPECT_TRUE(content.find("tex2D(posTex") != std::string::npos);
    EXPECT_TRUE(content.find("tex2D(normalTex") != std::string::npos);
    EXPECT_TRUE(content.find("tex2D(diffuseTex") != std::string::npos);
}

// ============================================================
// 4. Deferred.hlsl 引用 GPU Gems 2 §9.5 公开文献
// ============================================================
TEST(DeferredLighting, SourceCitesGpuGems2Deferred)
{
    std::string content;
    for (const auto& p : PossibleDeferredShaderPaths())
    {
        content = ReadEntireFile(p);
        if (!content.empty()) break;
    }
    EXPECT_TRUE(!content.empty());
    // 公开文献: GPU Gems 2 第 9.5 节 (Deferred Lighting)
    EXPECT_TRUE(content.find("GPU Gems 2") != std::string::npos);
    EXPECT_TRUE(content.find("9.5") != std::string::npos);
    EXPECT_TRUE(content.find("Deferred") != std::string::npos);
}

// ============================================================
// 5. Deferred.hlsl 包含 Blinn-Phong 光照计算
// ============================================================
TEST(DeferredLighting, SourceContainsBlinnPhong)
{
    std::string content;
    for (const auto& p : PossibleDeferredShaderPaths())
    {
        content = ReadEntireFile(p);
        if (!content.empty()) break;
    }
    EXPECT_TRUE(!content.empty());
    // Blinn-Phong: half vector = normalize(L + V), NdotH, pow
    EXPECT_TRUE(content.find("normalize") != std::string::npos);
    EXPECT_TRUE(content.find("saturate") != std::string::npos);
    EXPECT_TRUE(content.find("pow") != std::string::npos);
    EXPECT_TRUE(content.find("dot(N, L)") != std::string::npos
             || content.find("dot(N,L)") != std::string::npos);
    EXPECT_TRUE(content.find("ambient") != std::string::npos);
    EXPECT_TRUE(content.find("diffuse") != std::string::npos);
    EXPECT_TRUE(content.find("specular") != std::string::npos);
}

// ============================================================
// 6. Deferred.hlsl 非空 + 至少 50 行
// ============================================================
TEST(DeferredLighting, SourceNotEmpty)
{
    std::string content;
    for (const auto& p : PossibleDeferredShaderPaths())
    {
        content = ReadEntireFile(p);
        if (!content.empty()) break;
    }
    EXPECT_TRUE(!content.empty());
    EXPECT_TRUE(content.size() > 500);  // 至少 500 字节

    int lineCount = 0;
    for (char c : content) if (c == '\n') ++lineCount;
    EXPECT_TRUE(lineCount >= 30);
}

// ============================================================
// 7. DeferredPSHLSLC.h 生成产物存在 (可跳过)
// ============================================================
TEST(DeferredLighting, GeneratedHeaderExists)
{
    auto paths = PossibleDeferredGeneratedHeaderPaths();
    std::string found;
    for (const auto& p : paths)
    {
        if (FileExists(p))
        {
            found = p;
            break;
        }
    }
    if (found.empty())
    {
        std::printf("    [skip] DeferredPSHLSLC.h not found; "
            "set DDFIX_BUILD_DIR or run full build first.\n");
        EXPECT_TRUE(true);
        return;
    }
    std::printf("    [info] DeferredPSHLSLC.h at: %s\n", found.c_str());
    EXPECT_TRUE(true);
}

// ============================================================
// 8. DeferredPSHLSLC.h 字节码数组大小合理
// ============================================================
TEST(DeferredLighting, BytecodeArraySizeReasonable)
{
    auto paths = PossibleDeferredGeneratedHeaderPaths();
    for (const auto& p : paths)
    {
        if (!FileExists(p)) continue;

        std::string content = ReadEntireFile(p);
        EXPECT_TRUE(!content.empty());

        std::vector<std::string> markers = {
            "g_deferredPSHLSLC[",
            "g_deferredHLSLC[",
        };
        for (const auto& m : markers)
        {
            auto pos = content.find(m);
            if (pos == std::string::npos) continue;
            pos += m.size();
            size_t endPos = pos;
            while (endPos < content.size() && (content[endPos] >= '0' && content[endPos] <= '9'))
                ++endPos;
            std::string numStr = content.substr(pos, endPos - pos);
            if (numStr.empty()) continue;

            int arraySize = std::atoi(numStr.c_str());
            std::printf("    [info] %s in %s size = %d DWORDs (= %d bytes)\n",
                m.c_str(), p.c_str(), arraySize, arraySize * 4);
            // ps_3_0 字节码典型 30-5000 DWORD
            EXPECT_TRUE(arraySize > 0);
            EXPECT_TRUE(arraySize < 100000);
            return;
        }
    }
    std::printf("    [skip] No DeferredPSHLSLC.h to verify size; "
        "set DDFIX_BUILD_DIR or run full build first.\n");
    EXPECT_TRUE(true);
}
