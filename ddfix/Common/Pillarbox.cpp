#include "Pillarbox.h"

namespace NDDFIX
{
	namespace Render
	{

		PillarboxRect CalculatePillarbox(int bbWidth, int bbHeight, int displayWidth, int displayHeight)
		{
			PillarboxRect result = { 0, 0, 0, 0 };

			// 边界检查：任一参数 <= 0 时返回全零，调用方回退到全屏。
			// WHY: 防止除零 + 在 D3D9 设备未初始化时（bbSize 仍为 0）安全 no-op。
			if (bbWidth <= 0 || bbHeight <= 0 || displayWidth <= 0 || displayHeight <= 0)
			{
				return result;
			}

			// 使用 double 防止大分辨率整数溢出（int32 约 2.1e9 仍能装下 4096*4096，
			// 但 8K（7680*4320）会接近边界；double 更稳）。
			const double bbAspect = static_cast<double>(bbWidth) / static_cast<double>(bbHeight);
			const double dispAspect = static_cast<double>(displayWidth) / static_cast<double>(displayHeight);

			if (bbAspect > dispAspect)
			{
				// Back buffer 更宽（典型 16:9 内容渲染到 4:3 显示器）。
				// 按显示器宽度填满，高度按宽高比缩放，上下黑边（letterbox）。
				result.dstWidth = displayWidth;
				result.dstHeight = static_cast<int>(static_cast<double>(displayWidth) / bbAspect);
				result.dstX = 0;
				result.dstY = (displayHeight - result.dstHeight) / 2;
			}
			else
			{
				// Back buffer 更高或宽高比相等（典型 4:3 内容渲染到 16:9 显示器）。
				// 按显示器高度填满，宽度按宽高比缩放，左右黑边（pillarbox）。
				// 当 bbAspect == dispAspect 时，dstWidth == displayWidth, dstX == 0（无黑边）。
				result.dstHeight = displayHeight;
				result.dstWidth = static_cast<int>(static_cast<double>(displayHeight) * bbAspect);
				result.dstY = 0;
				result.dstX = (displayWidth - result.dstWidth) / 2;
			}

			return result;
		}

	} // namespace Render
} // namespace NDDFIX
