// ExecuteBufferParser.h - Phase 9.3 ExecuteBuffer 解析基础设施
//
// 设计目标：
//   1. 解析 D3D ExecuteBuffer 字节流（DirectX 6/7 SDK 公开格式）
//   2. 输出 ExtractedGeometry 中间表示，给 Phase 9.3.8+ GBuffer/Deferred 消费
//   3. 纯 STL 实现 + Wine d3dtypes.h 中已定义的 dx6::D3DINSTRUCTION / D3DTRIANGLE
//      / D3DPROCESSVERTICES / D3DSTATE 等结构（仅在 .cpp 内部使用，.h 保持类型无关）
//   4. 支持的 opcodes：
//      D3DOP_TRIANGLE (3), D3DOP_MATRIXLOAD (4), D3DOP_STATERENDER (8),
//      D3DOP_PROCESSVERTICES (9), D3DOP_TEXTURELOAD (10), D3DOP_EXIT (11)
//   5. 不支持/未实现的 opcode（如 POINT/LINE/MATRIXMULTIPLY/STATETRANSFORM/
//      STATELIGHT/BRANCHFORWARD/SPAN/SETSTATUS）会优雅跳过，记录日志，不崩
//
// 基于: DirectX 6/7 SDK 公开文档（D3DOP_* 指令流格式） + Wine d3dtypes.h
//       严格 Clean Room：不参考 6.5.9 .fx 源码
//
// .h 依赖关系（重要）：
//   - 不 include 任何 ddraw 头，让 ddfixtests 单元测试可以纯 STL 编译
//   - RenderState::state 用 DWORD 而非 dx6::D3DRENDERSTATETYPE（值相同）
//   - .cpp 才 include ddraw.h 使用 dx6:: 类型

#pragma once

// Phase 9.3: 引入 Windows 基础类型 (BYTE / WORD / DWORD) 供 ExtractedGeometry /
//   ParseStats 使用。故意不 include ddraw.h, 让 ddfixtests 单元测试可纯 STL
//   + Win32 编译, 不拖入整个 COM/D3D9 依赖。
//   替代方案是 typedef BYTE/WORD/DWORD 自己声明, 但与 windows.h 风格一致更
//   易读, 且 <windows.h> 头保护保证不会重复定义冲突。
#include <windows.h>

#include <cstddef>
#include <vector>

// Phase 9.3: ExecuteBufferParser 的输出数据结构（ExtractedGeometry）
// 这些结构后续会由 GBuffer / Deferred 渲染管线消费（Phase 9.3.8+）
namespace NDDFIX
{
namespace Render
{

struct ExtractedGeometry
{
	// 顶点（来自 ExecuteBuffer 内嵌 vertex buffer）
	// 字段命名沿用 d3d9types.h 的 D3DVERTEX 风格（位置 + 法线 + 纹理 + diffuse）
	struct Vertex
	{
		float x, y, z;        // 位置
		float nx, ny, nz;     // 法线
		float u, v;           // 纹理坐标（1 组）
		DWORD diffuse;        // 顶点色
	};

	// 4x4 矩阵（世界 / 视图 / 投影等）
	struct Matrix4x4
	{
		float m[4][4];
	};

	// 三角形（v1/v2/v3 是顶点索引，flags 是 triangle flags）
	struct Triangle
	{
		DWORD v1, v2, v3;
		DWORD flags;
	};

	// 纹理句柄（后续 Phase 9.3.8+ 查表得到 IDirect3DTexture2 / D3D9 texture）
	struct TextureHandle
	{
		DWORD handle;
		// 后续扩展: 存 m_IDirect3DTexture2* / IDirect3DBaseTexture9* 指针
	};

	// 渲染状态（state 是 dx6::D3DRENDERSTATETYPE 枚举值, DWORD 等价;
	//          显式用 DWORD 而非 enum, 让 .h 不依赖 ddraw.h）
	struct RenderState
	{
		DWORD state;   // dx6::D3DRENDERSTATETYPE 枚举值
		DWORD value;   // 状态值
	};

	// 顶点变换/光照指令
	struct ProcessVertexOp
	{
		DWORD dwFlags;
		DWORD wStart;        // WORD 在 DWORD 容器中（DWORD 兼容）
		DWORD wDest;         // 同上
		DWORD dwCount;
		DWORD dwReserved;
	};

	std::vector<Vertex>           vertices;
	std::vector<Triangle>         triangles;
	std::vector<Matrix4x4>        matrices;
	std::vector<TextureHandle>    textures;
	std::vector<RenderState>      renderStates;
	std::vector<ProcessVertexOp>  processVertexOps;
};

// Phase 9.3.1: ExecuteBuffer 字节流解析器
//
// 公开接口：
//   - Parse(bufferData, bufferSize, out): 主入口，解析整个 buffer
//   - GetLastError(): 获取上一次 Parse 的错误描述（用于日志/调试）
//   - 重置 Parse 状态：构造新对象即可（无状态成员，零分配）
//
// 用法:
//   ExecuteBufferParser parser;
//   ExtractedGeometry out;
//   if (parser.Parse(buffer, size, out)) {
//       // 成功：消费 out.triangles / out.vertices / out.renderStates ...
//   } else {
//       // 失败：log parser.GetLastError()
//   }
class ExecuteBufferParser
{
public:
	ExecuteBufferParser();
	~ExecuteBufferParser();

	// 主入口：解析 D3D ExecuteBuffer 字节流
	//
	// 参数:
	//   bufferData - 指向 ExecuteBuffer 指令流的指针（来自 LPD3DEXECUTEBUFFERDESC.lpData）
	//   bufferSize - 指令流总字节数（来自 LPD3DEXECUTEBUFFERDESC.dwBufferSize）
	//   out        - 输出 ExtractedGeometry（解析前应清空）
	//
	// 返回:
	//   true  - 成功解析（即使部分 op 被跳过）
	//   false - 数据损坏（size 不对齐、buffer 越界、非法 opcode 等）
	//
	// 失败策略:
	//   1. 越界访问立即返 false，不部分写 out
	//   2. 未知 opcode 跳过该指令流（D3DOP_* enum 值越大越要保守）
	//   3. 所有失败都设置 m_lastError，调用方 GetLastError() 可读
	bool Parse(const void* bufferData, DWORD bufferSize, ExtractedGeometry& out);

	// 测试用：手动注入外部 vertex 数据源（默认 nullptr 走 buffer 自身）
	//
	// WHY: ExecuteBuffer 的 vertex 数据实际放在 lpData 的 dwVertexOffset 处。
	//   Phase 9.3.1 解析阶段我们只解析 instruction 流 + 提取 handle，
	//   真正的 vertex 解析留到 Phase 9.3.8+ GBuffer 阶段再展开。
	//   这里提供注入接口，方便单元测试 fixture 字节流时不必关心 vertex 数据布局。
	void SetVertexData(const void* data, DWORD size)
	{
		m_vertexData = data;
		m_vertexDataSize = size;
	}

	// 取最后一次 Parse 失败的错误描述（中文，给 logf 用）
	const char* GetLastError() const { return m_lastError; }

	// 解析过程统计（用于性能分析 / 调试 HUD）
	struct ParseStats
	{
		DWORD instructionsParsed;     // 已解析的指令数
		DWORD trianglesExtracted;     // 提取的三角形数
		DWORD matricesExtracted;      // 提取的矩阵数
		DWORD stateChangesExtracted;  // 提取的 render state 数
		DWORD processVertexOpsExtracted; // 提取的 process vertices op 数
		DWORD texturesExtracted;      // 提取的 texture handle 数
		DWORD unknownOpsSkipped;      // 跳过的未知 op 数
		DWORD bytesConsumed;          // 实际消费的字节数
	};
	const ParseStats& GetStats() const { return m_stats; }

private:
	// 错误信息
	void SetError(const char* msg) { m_lastError = msg; }

	// 状态
	const char* m_lastError;
	ParseStats  m_stats;
	const void* m_vertexData;
	DWORD       m_vertexDataSize;
};

} // namespace Render
} // namespace NDDFIX
