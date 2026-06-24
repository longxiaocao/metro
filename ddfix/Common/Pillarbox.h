#pragma once

// ======================================================================
// Phase 9.1.3: Pillarbox 居中计算
// ======================================================================
//
// 设计目标
// --------
//   当 BackBuffer 宽高比（典型 4:3 游戏内部分辨率）与显示器宽高比
//   （典型 16:9 物理分辨率）不一致时，把 BackBuffer 内容居中拉伸到
//   显示器中央，两侧留出黑边（pillarbox）。反向则上下留黑边（letterbox）。
//
// 用途
// ----
//   在 IDirectDrawSurface4::Blt 渲染到 Primary surface 时调用，
//   用返回的 dest 区域替换默认的全屏 dest rect。
//
// 与 GPU 上采样概念的关系
// ----------------------
//   Pillarbox 实际是 Direct3D 9 Sprite / 1.4.x SetTextureStageState
//   + D3DXMatrixTransformation2D 的"局部 view 区域"概念：
//   - Sprite Draw 时传入 srcRect（源纹理区域）+ 一个缩放矩阵
//   - 缩放矩阵的目标区域由调用方决定
//   - 居中 = 让目标区域相对显示分辨率有偏移
//   详见 D3D9 文档 D3DXMatrixTransformation2D / ID3DXSprite::Draw
//   公开文献引用见 docs/literature/PUBLIC_LITERATURE.md
//
// Clean Room
// ----------
//   本实现仅基于公开 D3D9 API 文档与基础宽高比数学推导，
//   不引用 6.5.9 内部实现细节。
// ======================================================================

namespace NDDFIX
{
	namespace Render
	{

		// Pillarbox 输出矩形：在显示器坐标空间内的居中区域
		struct PillarboxRect
		{
			int dstX;       // 输出 dest 区域起点 X
			int dstY;       // 输出 dest 区域起点 Y
			int dstWidth;   // 输出 dest 区域宽度
			int dstHeight;  // 输出 dest 区域高度
		};

		// 计算 Pillarbox 居中区域
		//
		// 参数:
		//   bbWidth, bbHeight   - Back buffer 宽高（游戏内部渲染分辨率）
		//   displayWidth, displayHeight - 显示器物理分辨率
		//
		// 返回:
		//   居中后的 dest 区域（dstWidth/dstHeight 之一等于 display 的对应维度，
		//   另一边按 back buffer 宽高比缩放）
		//
		// 边界情况:
		//   - 任一参数 <= 0：返回全零 Rect（调用方应回退到全屏）
		//   - bbAspect == dispAspect：dstX=0, dstY=0, dstWidth=displayWidth,
		//     dstHeight=displayHeight（无黑边）
		//
		// 算法（教科书标准 aspect ratio 适配，公开文献常见做法）:
		//   - bbAspect = bbWidth / bbHeight
		//   - dispAspect = displayWidth / displayHeight
		//   - 若 bbAspect > dispAspect（back buffer 更宽）:
		//       letterbox（上下黑边）: dstWidth = displayWidth,
		//                              dstHeight = displayWidth / bbAspect
		//   - 否则（back buffer 更高）:
		//       pillarbox（左右黑边）: dstHeight = displayHeight,
		//                              dstWidth = displayHeight * bbAspect
		//   - 居中偏移 = (display - dst) / 2
		PillarboxRect CalculatePillarbox(int bbWidth, int bbHeight, int displayWidth, int displayHeight);

	} // namespace Render
} // namespace NDDFIX
