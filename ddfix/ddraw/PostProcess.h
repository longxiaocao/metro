// PostProcess.h - Phase 9.2 SMAA 抗锯齿后处理调度器
//
// 设计目标：
//   - 单一职责：在 IDirectDrawSurface4::Blt 渲染到 Primary 之后，对
//     BackBuffer 内容跑 SMAA 后处理（写回 BackBuffer），再 Present。
//   - 单例：dll 范围内一份，集成简单。
//   - 失败容错：任何步骤失败（编译 shader / 创 RT / 创 texture 失败），
//     自动降级为 Off 模式，绝不阻断 Blt 路径。
//   - 单元测试友好：Mode 字符串解析等纯逻辑函数暴露为 static，不依赖
//     任何 D3D9 设备状态，ddfixtests 链接 ddfix-static 即可测。
//
// 集成点：IDirectDrawSurface4::Blt 在 m_surfaceType==Primary 渲染完成
//   之后、调 Present 之前调 PostProcess::Instance()->Run(...)。Mode=Off
//   时 Run() 内部直接返 S_OK，零开销。
//
// 模式语义（与 ConfigManager::PostProcessConfig.smaaMode 对应）：
//   0 = Off      (no-op，零开销)
//   1 = Low      (60% quality, 4 search steps,  无 diagonal)
//   2 = Medium   (80% quality, 8 search steps,  无 diagonal, 默认推荐)
//   3 = High     (95% quality, 16 search steps, 8 diagonal)
//   4 = Ultra    (99% quality, 32 search steps, 16 diagonal)
//
// 与 SMAA 官方一致；Preset 选哪个完全由 SMAA.h 宏决定（运行时通过
// 重新编译 shader 切换，当前实现固定走 Medium preset，Mode 仅控制
// PostProcess 调度器是否启用）。

#pragma once

#include <d3d9.h>
#include <d3dx9.h>  // ID3DXConstantTable
#include <ostream>  // operator<< for Mode

namespace NDDFIX
{
namespace PostProcess
{

// SMAA 抗锯齿质量等级
enum class Mode
{
    Off    = 0,
    Low    = 1,
    Medium = 2,
    High   = 3,
    Ultra  = 4
};

// 把整数 0..4 转换为 Mode 枚举；越界返 Mode::Off（fail-safe）。
// WHY: ConfigManager 解析 INI 得到的 int 0..4 可能越界；调方期望降级。
Mode ModeFromInt(int v);

// 把 Mode 枚举转换为可读字符串（"Off" / "Low" / "Medium" / "High" / "Ultra"）。
// 仅供日志 / HUD 显示；调方负责 buffer 长度。
const char* ModeToString(Mode m);

// 把字符串解析为 Mode 枚举（大小写不敏感）。
// 接受 "Off"/"0"/"Low"/"1"/.../ "Ultra"/"4"；无效值返 Mode::Off。
Mode ModeFromString(const char* s);

// 让 ostream 能直接打印 Mode（单元测试 EXPECT_EQ 失败诊断用）。
// 输出形如 "Mode::Medium"（与 ModeToString 的简短形式互补）。
inline std::ostream& operator<<(std::ostream& os, Mode m)
{
    return os << "Mode::" << ModeToString(m);
}

class PostProcess
{
public:
    static PostProcess* Instance();

    // 初始化 SMAA 子系统（D3D9 设备就绪后调一次）。
    //   - 编译 3 个 PS（EdgeDetection / BlendingWeight / NeighborhoodBlending）
    //   - 创建 2 张临时 RT（edgesTex / blendTex）和预计算纹理
    //   - 任何步骤失败 → 内部标记为 unavailable，Run() 退化为 Off 行为
    //   - 多次调用安全：先 Shutdown 再 Initialize
    HRESULT Initialize(IDirect3DDevice9* device);

    // 释放所有 D3D9 资源；调方在 Reset 设备 / 卸载 ddfix 时调。
    void Shutdown();

    // 抗锯齿主入口。Mode==Off 或 unavailable → 直接返 S_OK。
    //   - inputRT / outputRT 通常都是 BackBuffer（就地处理）
    //   - 失败时返 E_FAIL，但不影响 Blt 路径（调方应忽略返回值）
    HRESULT Run(IDirect3DDevice9* device,
                IDirect3DSurface9* inputRT,
                IDirect3DSurface9* outputRT,
                Mode mode);

    // 模式读写（运行期可改）
    void SetMode(Mode m) { m_mode = m; }
    Mode GetMode() const  { return m_mode; }

    // 单元测试用：检查 Initialize 是否成功（true = SMAA 可用）。
    bool IsAvailable() const { return m_available; }

private:
    PostProcess();
    ~PostProcess();
    PostProcess(const PostProcess&) = delete;
    PostProcess& operator=(const PostProcess&) = delete;

    // 内部：编译 PS + 创建 RT + 创建预计算纹理。
    HRESULT CreateSMAAResources(IDirect3DDevice9* device, int width, int height);
    void ReleaseSMAAResources();

    // 内部：3 趟 Pass（edgeTex → blendTex → output）
    HRESULT EdgeDetectionPass(IDirect3DDevice9* device, IDirect3DSurface9* inputRT);
    HRESULT BlendingWeightsPass(IDirect3DDevice9* device);
    HRESULT NeighborhoodBlendingPass(IDirect3DDevice9* device, IDirect3DSurface9* outputRT);

    Mode m_mode = Mode::Off;
    int  m_width = 0;
    int  m_height = 0;
    bool m_available = false;   // 初始化成功标记

    // SMAA 资源（裸指针，由 d3d9 设备管理；Reset 时自动失效）
    IDirect3DTexture9*       m_edgeTex       = nullptr;
    IDirect3DTexture9*       m_blendTex      = nullptr;
    IDirect3DTexture9*       m_searchTex     = nullptr;
    IDirect3DTexture9*       m_areaTex       = nullptr;
    IDirect3DSurface9*       m_edgeSurface   = nullptr;
    IDirect3DSurface9*       m_blendSurface  = nullptr;
    IDirect3DPixelShader9*   m_edgeShader    = nullptr;
    IDirect3DPixelShader9*   m_blendShader   = nullptr;
    IDirect3DPixelShader9*   m_neighborShader = nullptr;
    ID3DXConstantTable*      m_edgeConst     = nullptr;
    ID3DXConstantTable*      m_blendConst    = nullptr;
    ID3DXConstantTable*      m_neighborConst = nullptr;
};

} // namespace PostProcess
} // namespace NDDFIX
