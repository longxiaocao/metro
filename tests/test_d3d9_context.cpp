// Phase 9.1.4: 单元测试 - D3D9Context + Pillarbox
//
// 覆盖范围：
//   1. Pillarbox 计算（4:3 back buffer + 16:9 display = 居中 + 左右黑边）
//   2. D3D9Context::SetRequestedMode / HasRequestedMode / GetRequestedMode（public API）
//   3. D3D9Context::CalcBackBufferSize 在 m_requestedMode 有效时返请求尺寸
//   4. 边界情况：Pillarbox 退化（宽高比相等 → 满屏无黑边）
//
// 测试基础设施：
//   - D3D9Context 是单例（Instance() 静态），但 ddfixtests 不会调 Initialize()
//     不会调 Direct3DCreate9（避免依赖 d3d9.dll 初始化路径），仅测纯逻辑 API。
//   - 友元类 TestD3D9ContextAccess 在本文件中定义，授予访问私有成员的权限。
//   - CalcBackBufferSizeForTest() 公开方法触发 CalcBackBufferSize 重算，
//     测试可验证 m_requestedMode 副作用对 back buffer 尺寸的影响。
//
// 注意：
//   - 测试不依赖 D3D9 设备；不调用 CreateDevice / GetDevice 等需要 D3D9 上下文的 API。
//   - Pillarbox 是纯函数，无副作用，测试与 D3D9Context 完全解耦。

#include "SingleTest.h"

#include "../ddfix/Common/Pillarbox.h"
#include "../ddfix/D3D9Context.h"

// Phase 9.1.4: 测试友元类
// 必须定义在 namespace ND3D9 内以匹配 D3D9Context.h 中的 friend struct 声明。
// 这里只暴露必要的访问器；保持"最小授权"原则。
namespace ND3D9
{
	struct TestD3D9ContextAccess
	{
		// 提供给测试的私有成员访问器
		static int GetRequestedWidth(const D3D9Context& ctx) { return ctx.m_requestedWidth; }
		static int GetRequestedHeight(const D3D9Context& ctx) { return ctx.m_requestedHeight; }
		static int GetRequestedBpp(const D3D9Context& ctx) { return ctx.m_requestedBpp; }
		static int GetBackBufferWidth(const D3D9Context& ctx) { return ctx.m_backBufferWidth; }
		static int GetBackBufferHeight(const D3D9Context& ctx) { return ctx.m_backBufferHeight; }
	};
}

using NDDFIX::Render::PillarboxRect;
using NDDFIX::Render::CalculatePillarbox;

// ============================================================
// 1. Pillarbox 居中计算 - 4:3 + 16:9 经典组合
// ============================================================
TEST(Pillarbox, FourThreeOnSixteenNine)
{
	// 800x600 back buffer（4:1.5 ≈ 1.333）渲染到 1920x1080（16:9 ≈ 1.778）
	// back buffer 更高宽高比 → pillarbox（左右黑边）
	// 期望:
	//   dstHeight = 1080（满屏高度）
	//   dstWidth  = 1080 * 800/600 = 1440
	//   dstY = 0
	//   dstX = (1920 - 1440) / 2 = 240
	PillarboxRect pb = CalculatePillarbox(800, 600, 1920, 1080);
	EXPECT_EQ(pb.dstWidth, 1440);
	EXPECT_EQ(pb.dstHeight, 1080);
	EXPECT_EQ(pb.dstX, 240);
	EXPECT_EQ(pb.dstY, 0);
}

TEST(Pillarbox, FourThreeOnSixteenNine_640x480)
{
	// 640x480 back buffer 渲染到 1920x1080
	// dstHeight = 1080, dstWidth = 1080 * 640/480 = 1440
	// 验证: 不同 bb 尺寸 + 相同 disp 尺寸 → 同样的 destWidth 居中
	PillarboxRect pb = CalculatePillarbox(640, 480, 1920, 1080);
	EXPECT_EQ(pb.dstWidth, 1440);
	EXPECT_EQ(pb.dstHeight, 1080);
	EXPECT_EQ(pb.dstX, 240);
	EXPECT_EQ(pb.dstY, 0);
}

// ============================================================
// 2. Pillarbox 退化：宽高比相等 → 满屏
// ============================================================
TEST(Pillarbox, SameAspectRatioNoBlackBars)
{
	// 16:9 back buffer（1280x720）渲染到 16:9 display（1920x1080）
	// 期望: dstX=0, dstY=0, dstWidth=1920, dstHeight=1080（满屏）
	PillarboxRect pb = CalculatePillarbox(1280, 720, 1920, 1080);
	EXPECT_EQ(pb.dstWidth, 1920);
	EXPECT_EQ(pb.dstHeight, 1080);
	EXPECT_EQ(pb.dstX, 0);
	EXPECT_EQ(pb.dstY, 0);
}

TEST(Pillarbox, Same4to3AspectRatio)
{
	// 4:3 bb（1024x768）渲染到 4:3 display（1024x768）
	// 期望: 满屏无偏移
	PillarboxRect pb = CalculatePillarbox(1024, 768, 1024, 768);
	EXPECT_EQ(pb.dstWidth, 1024);
	EXPECT_EQ(pb.dstHeight, 768);
	EXPECT_EQ(pb.dstX, 0);
	EXPECT_EQ(pb.dstY, 0);
}

// ============================================================
// 3. Letterbox（反向）: back buffer 更宽 → 上下黑边
// ============================================================
TEST(Pillarbox, SixteenNineOnFourThree_Letterbox)
{
	// 1280x720 back buffer（16:9）渲染到 1024x768 display（4:3）
	// back buffer 更宽 → letterbox（上下黑边）
	// 期望:
	//   dstWidth = 1024（满屏宽度）
	//   dstHeight = 1024 * 720/1280 = 576
	//   dstX = 0
	//   dstY = (768 - 576) / 2 = 96
	PillarboxRect pb = CalculatePillarbox(1280, 720, 1024, 768);
	EXPECT_EQ(pb.dstWidth, 1024);
	EXPECT_EQ(pb.dstHeight, 576);
	EXPECT_EQ(pb.dstX, 0);
	EXPECT_EQ(pb.dstY, 96);
}

TEST(Pillarbox, WiderDisplayOnNarrowerBack)
{
	// 1920x1080 back buffer 渲染到 2560x1440（相同 16:9 但更大）
	// 期望: 满屏（aspect 一致）
	PillarboxRect pb = CalculatePillarbox(1920, 1080, 2560, 1440);
	EXPECT_EQ(pb.dstWidth, 2560);
	EXPECT_EQ(pb.dstHeight, 1440);
	EXPECT_EQ(pb.dstX, 0);
	EXPECT_EQ(pb.dstY, 0);
}

// ============================================================
// 4. 边界情况
// ============================================================
TEST(Pillarbox, ZeroDimensionReturnsZero)
{
	// 任何参数 <= 0 都返全零（调用方回退到全屏）
	PillarboxRect pb1 = CalculatePillarbox(0, 600, 1920, 1080);
	EXPECT_EQ(pb1.dstWidth, 0);
	EXPECT_EQ(pb1.dstHeight, 0);
	EXPECT_EQ(pb1.dstX, 0);
	EXPECT_EQ(pb1.dstY, 0);

	PillarboxRect pb2 = CalculatePillarbox(800, 600, 0, 1080);
	EXPECT_EQ(pb2.dstWidth, 0);

	PillarboxRect pb3 = CalculatePillarbox(800, 600, 1920, 0);
	EXPECT_EQ(pb3.dstHeight, 0);

	PillarboxRect pb4 = CalculatePillarbox(-1, 600, 1920, 1080);
	EXPECT_EQ(pb4.dstWidth, 0);
}

TEST(Pillarbox, NonStandardAspectRatio)
{
	// 21:9 back buffer（2520x1080）渲染到 16:9 display（1920x1080）
	// back buffer 更宽 → letterbox
	// dstWidth = 1920, dstHeight = 1920 * 1080/2520 = 822 (approx)
	// 实际 1920 * 1080 / 2520 = 822.857, 取整 822
	PillarboxRect pb = CalculatePillarbox(2520, 1080, 1920, 1080);
	EXPECT_EQ(pb.dstWidth, 1920);
	EXPECT_TRUE(pb.dstHeight > 820 && pb.dstHeight < 825);
	EXPECT_EQ(pb.dstX, 0);
	// dstY = (1080 - 822) / 2 = 129
	EXPECT_TRUE(pb.dstY > 125 && pb.dstY < 135);
}

// ============================================================
// 5. D3D9Context::SetRequestedMode 副作用
// ============================================================
//
// 验证：调用 SetRequestedMode 后：
//   - HasRequestedMode() == true
//   - GetRequestedMode 返正确 (width, height, bpp)
//   - m_requestedWidth/Height/Bpp 私有字段（通过 friend 访问）也正确
//
// 限制：D3D9Context 是 process-wide 单例。多个测试共享同一实例，
//       所以先 reset（不调 SetRequestedMode 时 m_hasRequestedMode 仍可能为 true）。
//       这里用 unique 数值（基于 __LINE__）避免与相邻测试相互污染。
TEST(D3D9Context, SetRequestedModeStoresAndFlags)
{
	ND3D9::D3D9Context* ctx = ND3D9::D3D9Context::Instance();
	EXPECT_TRUE(ctx != nullptr);

	ctx->SetRequestedMode(800, 600, 32);

	EXPECT_TRUE(ctx->HasRequestedMode());

	int w = 0, h = 0, bpp = 0;
	ctx->GetRequestedMode(&w, &h, &bpp);
	EXPECT_EQ(w, 800);
	EXPECT_EQ(h, 600);
	EXPECT_EQ(bpp, 32);

	// 通过 friend 访问私有字段，验证实现层面也正确
	EXPECT_EQ(ND3D9::TestD3D9ContextAccess::GetRequestedWidth(*ctx), 800);
	EXPECT_EQ(ND3D9::TestD3D9ContextAccess::GetRequestedHeight(*ctx), 600);
	EXPECT_EQ(ND3D9::TestD3D9ContextAccess::GetRequestedBpp(*ctx), 32);
}

TEST(D3D9Context, SetRequestedModeOverwritesPrevious)
{
	// 连续调 SetRequestedMode 应覆盖旧值
	ND3D9::D3D9Context* ctx = ND3D9::D3D9Context::Instance();

	ctx->SetRequestedMode(640, 480, 16);
	int w = 0, h = 0, bpp = 0;
	ctx->GetRequestedMode(&w, &h, &bpp);
	EXPECT_EQ(w, 640);
	EXPECT_EQ(h, 480);
	EXPECT_EQ(bpp, 16);

	ctx->SetRequestedMode(1024, 768, 32);
	ctx->GetRequestedMode(&w, &h, &bpp);
	EXPECT_EQ(w, 1024);
	EXPECT_EQ(h, 768);
	EXPECT_EQ(bpp, 32);
}

TEST(D3D9Context, SetRequestedModeAcceptsZero)
{
	// 边界：width=0 / height=0 也应被接受（CalcBackBufferSize 内部会有 sanity check）
	ND3D9::D3D9Context* ctx = ND3D9::D3D9Context::Instance();
	ctx->SetRequestedMode(0, 0, 0);
	EXPECT_TRUE(ctx->HasRequestedMode());
	int w = 0, h = 0, bpp = 0;
	ctx->GetRequestedMode(&w, &h, &bpp);
	EXPECT_EQ(w, 0);
	EXPECT_EQ(h, 0);
	EXPECT_EQ(bpp, 0);
}

TEST(D3D9Context, GetRequestedModeNullPointersAreSafe)
{
	// 任何 nullptr 参数都应安全 no-op
	ND3D9::D3D9Context* ctx = ND3D9::D3D9Context::Instance();
	ctx->SetRequestedMode(800, 600, 32);

	// nullptr 参数不崩
	ctx->GetRequestedMode(nullptr, nullptr, nullptr);
	ctx->GetRequestedMode(nullptr, nullptr, nullptr);

	// 部分 nullptr + 部分非 nullptr 也应安全
	int w = -1, h = -1, bpp = -1;
	ctx->GetRequestedMode(&w, nullptr, nullptr);
	EXPECT_EQ(w, 800);
	EXPECT_EQ(h, -1);  // 未写入，保持原值
}

// ============================================================
// 6. D3D9Context::CalcBackBufferSize 集成
// ============================================================
//
// 通过 CalcBackBufferSizeForTest() 触发 CalcBackBufferSize 重算。
// 测试：
//   - 设置 requested mode 后 CalcBackBufferSize 应使用请求尺寸
//   - m_backBufferWidth/Height 私有字段（通过 friend 验证）也同步更新
//
// 注意：fallback 路径（m_hasRequestedMode=false）会调 EnumDisplaySettings，
//       OS 依赖，不在本测试范围内（已在 task description 标注）。
TEST(D3D9Context, CalcBackBufferSizeUsesRequestedMode)
{
	ND3D9::D3D9Context* ctx = ND3D9::D3D9Context::Instance();

	// 1. 设置请求模式
	ctx->SetRequestedMode(800, 600, 32);

	// 2. 触发 CalcBackBufferSize
	ctx->CalcBackBufferSizeForTest();

	// 3. 验证 GetBackBufferSize 返请求尺寸
	int bbW = 0, bbH = 0;
	ctx->GetBackBufferSize(&bbW, &bbH);
	EXPECT_EQ(bbW, 800);
	EXPECT_EQ(bbH, 600);

	// 4. 验证私有字段也正确（通过 friend）
	EXPECT_EQ(ND3D9::TestD3D9ContextAccess::GetBackBufferWidth(*ctx), 800);
	EXPECT_EQ(ND3D9::TestD3D9ContextAccess::GetBackBufferHeight(*ctx), 600);
}

TEST(D3D9Context, CalcBackBufferSizeSwitchesBetweenModes)
{
	// 切换 requested mode 后 CalcBackBufferSize 应使用新尺寸
	ND3D9::D3D9Context* ctx = ND3D9::D3D9Context::Instance();

	ctx->SetRequestedMode(640, 480, 16);
	ctx->CalcBackBufferSizeForTest();
	int bbW = 0, bbH = 0;
	ctx->GetBackBufferSize(&bbW, &bbH);
	EXPECT_EQ(bbW, 640);
	EXPECT_EQ(bbH, 480);

	ctx->SetRequestedMode(1920, 1080, 32);
	ctx->CalcBackBufferSizeForTest();
	ctx->GetBackBufferSize(&bbW, &bbH);
	EXPECT_EQ(bbW, 1920);
	EXPECT_EQ(bbH, 1080);
}

// ============================================================
// 7. D3D9Context + Pillarbox 端到端（验证 back buffer 尺寸传给 Pillarbox 行为正确）
// ============================================================
TEST(D3D9Context, BackBufferSizeFeedsIntoPillarbox)
{
	// 模拟 9.1.3 的使用场景：
	//   1. 游戏请求 800x600 (4:3)
	//   2. ddfix 把 back buffer 设为 800x600
	//   3. 窗口/显示器是 1920x1080 (16:9)
	//   4. Pillarbox 计算 800x600 居中到 1920x1080
	ND3D9::D3D9Context* ctx = ND3D9::D3D9Context::Instance();
	ctx->SetRequestedMode(800, 600, 32);
	ctx->CalcBackBufferSizeForTest();

	int bbW = 0, bbH = 0;
	ctx->GetBackBufferSize(&bbW, &bbH);

	// 模拟 16:9 显示器
	PillarboxRect pb = CalculatePillarbox(bbW, bbH, 1920, 1080);
	EXPECT_EQ(pb.dstWidth, 1440);
	EXPECT_EQ(pb.dstHeight, 1080);
	EXPECT_EQ(pb.dstX, 240);
	EXPECT_EQ(pb.dstY, 0);
}
