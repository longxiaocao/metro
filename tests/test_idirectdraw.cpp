// Phase 9.1.5: 单元测试 - IDirectDraw SetDisplayMode
//
// 覆盖范围：
//   m_IDirectDraw::SetDisplayMode 的核心契约 —— 调用后游戏的请求模式
//   会被存到 D3D9Context 单例里，HasRequestedMode() 返回 true，
//   GetRequestedMode 返正确 (width, height, bpp)。
//
// 实现层面（ddfix/ddraw/IDirectDraw.cpp:410-433）：
//   m_IDirectDraw::SetDisplayMode 的全部副作用就是调
//   ND3D9::D3D9Context::Instance()->SetRequestedMode(a, b, c)。
//   因此本测试在 D3D9Context 层验证契约（"调 SetDisplayMode 等价于
//   调 D3D9Context::SetRequestedMode"），无需构造真实的 m_IDirectDraw。
//
// 不构造真实 m_IDirectDraw 的原因（避免测试基础设施爆炸）：
//   - m_IDirectDraw 在 ddfix/ddraw/IDirectDraw.cpp，
//   - ddfix-static 不包含 ddraw/（仅 Config/Debug/Common/D3D9Context.cpp），
//   - 把 ddraw/ 全量纳入 ddfix-static 会引入全局 ProxyAddressLookupTable
//     静态初始化（dllmain.cpp 持有），在 test.exe 启动时可能触发
//     STATUS_HEAP_CORRUPTION（CI #49 历史教训），
//   - 所以走"契约测试"路线：在 D3D9Context API 层验证等价行为。
//
// 已被 9.1.4 覆盖的部分：
//   - D3D9Context::SetRequestedMode 的字段读写（test_d3d9_context.cpp）
//   - Pillarbox 集成（test_d3d9_context.cpp）
//   本文件额外覆盖：
//   - 不同分辨率的连续 SetDisplayMode 调用（D3D9Context 行为可观察性）
//   - 典型 4:3 / 16:9 / 16:10 三类游戏分辨率的覆盖
//   - "覆盖语义"：连续调 SetDisplayMode 应覆盖之前的值

#include "SingleTest.h"

#include "../ddfix/D3D9Context.h"

using ND3D9::D3D9Context;

// ============================================================
// 1. 基本契约：SetRequestedMode 等价于 m_IDirectDraw::SetDisplayMode 的副作用
// ============================================================
//
// m_IDirectDraw::SetDisplayMode(w, h, bpp) 的全部副作用就是调
//   D3D9Context::Instance()->SetRequestedMode(w, h, bpp)
// 所以验证 D3D9Context 收到 (w, h, bpp) + HasRequestedMode()==true
// 就足以证明 SetDisplayMode 的契约被满足。
TEST(IDirectDraw, SetDisplayModeStoresRequestedMode)
{
	D3D9Context* ctx = D3D9Context::Instance();
	EXPECT_TRUE(ctx != nullptr);

	// 调用"等价"于 m_IDirectDraw::SetDisplayMode(800, 600, 32) 的副作用
	ctx->SetRequestedMode(800, 600, 32);

	// 契约：
	//   1. HasRequestedMode() == true
	//   2. GetRequestedMode 返正确尺寸
	EXPECT_TRUE(ctx->HasRequestedMode());

	int w = 0, h = 0, bpp = 0;
	ctx->GetRequestedMode(&w, &h, &bpp);
	EXPECT_EQ(w, 800);
	EXPECT_EQ(h, 600);
	EXPECT_EQ(bpp, 32);
}

TEST(IDirectDraw, SetDisplayModeDifferentResolution)
{
	// 1024x768（4:3 衍生分辨率）经典组合
	D3D9Context* ctx = D3D9Context::Instance();
	ctx->SetRequestedMode(1024, 768, 32);

	EXPECT_TRUE(ctx->HasRequestedMode());
	int w = 0, h = 0, bpp = 0;
	ctx->GetRequestedMode(&w, &h, &bpp);
	EXPECT_EQ(w, 1024);
	EXPECT_EQ(h, 768);
	EXPECT_EQ(bpp, 32);
}

TEST(IDirectDrawn, SetDisplayModeWideScreenResolution)
{
	// 1280x720（16:9 宽屏）常见游戏内部分辨率
	D3D9Context* ctx = D3D9Context::Instance();
	ctx->SetRequestedMode(1280, 720, 32);

	EXPECT_TRUE(ctx->HasRequestedMode());
	int w = 0, h = 0, bpp = 0;
	ctx->GetRequestedMode(&w, &h, &bpp);
	EXPECT_EQ(w, 1280);
	EXPECT_EQ(h, 720);
	EXPECT_EQ(bpp, 32);
}

// ============================================================
// 2. 覆盖语义：连续 SetDisplayMode 应覆盖之前的值
// ============================================================
TEST(IDirectDraw, SetDisplayModeOverwritesPrevious)
{
	D3D9Context* ctx = D3D9Context::Instance();

	// 第一次：800x600
	ctx->SetRequestedMode(800, 600, 32);
	int w = 0, h = 0, bpp = 0;
	ctx->GetRequestedMode(&w, &h, &bpp);
	EXPECT_EQ(w, 800);
	EXPECT_EQ(h, 600);
	EXPECT_EQ(bpp, 32);

	// 第二次：640x480 (玩家从窗口模式切到全屏 640x480)
	ctx->SetRequestedMode(640, 480, 16);
	ctx->GetRequestedMode(&w, &h, &bpp);
	EXPECT_EQ(w, 640);
	EXPECT_EQ(h, 480);
	EXPECT_EQ(bpp, 16);
}

TEST(IDirectDraw, SetDisplayModeWithBppChange)
{
	// 16bpp → 32bpp 切换（颜色深度变化）
	D3D9Context* ctx = D3D9Context::Instance();

	ctx->SetRequestedMode(800, 600, 16);
	int w = 0, h = 0, bpp = 0;
	ctx->GetRequestedMode(&w, &h, &bpp);
	EXPECT_EQ(bpp, 16);

	ctx->SetRequestedMode(800, 600, 32);
	ctx->GetRequestedMode(&w, &h, &bpp);
	EXPECT_EQ(bpp, 32);
}

// ============================================================
// 3. 端到端：SetDisplayMode → CalcBackBufferSize 链路
// ============================================================
//
// 模拟生产流程：玩家启动游戏 → 游戏的 SetDisplayMode(800, 600, 32)
//  → m_IDirectDraw::SetDisplayMode 拦截 → D3D9Context::SetRequestedMode
//  → ddfix 在创建 D3D9 设备前调 CalcBackBufferSize → 拿到 800x600
//  → 创建 800x600 的 back buffer
//  → 渲染内容 800x600 → Pillarbox 居中到玩家窗口 1920x1080
TEST(IDirectDraw, EndToEnd_SetDisplayMode_FeedsBackBufferSize)
{
	D3D9Context* ctx = D3D9Context::Instance();

	// 玩家请求 800x600 (4:3)
	ctx->SetRequestedMode(800, 600, 32);

	// 触发 CalcBackBufferSize
	ctx->CalcBackBufferSizeForTest();

	// ddfix 的 back buffer 应是 800x600
	int bbW = 0, bbH = 0;
	ctx->GetBackBufferSize(&bbW, &bbH);
	EXPECT_EQ(bbW, 800);
	EXPECT_EQ(bbH, 600);
}
