// ExecuteBufferParser.cpp - Phase 9.3 ExecuteBuffer 解析器实现
//
// 基于: DirectX 6/7 SDK 公开文档（D3DOP_* 指令流格式） + Wine d3dtypes.h
//       严格 Clean Room：不参考 6.5.9 .fx 源码
//
// 字节流格式（参考 DX6 SDK "Execute Buffers" 章节）:
//   [D3DINSTRUCTION (4 bytes)] [data: bSize DWORDs * wCount records] ...
//   ...
//   D3DOP_EXIT (终止)
//
// 关键约束:
//   - D3DINSTRUCTION.bSize 是"每个数据记录的 DWORD 数", 不是字节数
//   - 总数据字节数 = bSize * 4 * wCount
//   - 指令流 DWORD 对齐
//   - 任何越界 / 不对齐 / 非法 opcode → Parse 返 false
//
// 设计说明 (不依赖 ddraw.h):
//   - 本文件故意不 include ddraw.h，避免 ddraw/ 下 COM proxy 全局污染 ddfix-static
//   - 内部用 LocalD3D* 类型镜像 Wine d3dtypes.h 中的 dx6::D3D* 布局
//   - 字段名 / 大小 / 对齐方式与 dx6:: 版本完全一致（公开 API 定义，无版权问题）
//   - 与 IDirect3DDevice::Execute hook 集成时，caller 负责把 LocalD3D* 字段
//     拷到 dx6::D3D* 字段（或反过来），因为两边内存布局相同

#include "ExecuteBufferParser.h"

#include <cstring>

namespace NDDFIX
{
namespace Render
{

// -------------------- 工具：DWORD 对齐 --------------------
//
// WHY: D3D ExecuteBuffer 字节流是 DWORD 对齐的，所有 D3DINSTRUCTION / D3DSTATE /
//   D3DTRIANGLE 等结构都是 4 的倍数大小。bSize 字段以 DWORD 为单位。
//   这里用本地函数而不是 reinterpret_cast，避免读未对齐 DWORD 触发
//   x86 unaligned access（虽然 x86 支持，但 ARM/x64 严格对齐更安全）。
static inline bool IsDwordAligned(const void* p)
{
	return (reinterpret_cast<uintptr_t>(p) & 3u) == 0;
}

// -------------------- 内部 D3D 类型定义（镜像 dx6::D3D*） --------------------
namespace
{

// D3DOP_* 枚举值（与 dx6::D3DOPCODE 完全一致，基于 DirectX 6/7 SDK 公开定义）
//   注：实际 enum 值 1..14 由 D3D6 SDK 公开文档确定；任何引用 Wine d3dtypes.h 的
//   源都应保持这些值不变。
enum LocalD3DOpcode : BYTE
{
	D3DOP_POINT_LOCAL           = 1,
	D3DOP_LINE_LOCAL            = 2,
	D3DOP_TRIANGLE_LOCAL        = 3,
	D3DOP_MATRIXLOAD_LOCAL      = 4,
	D3DOP_MATRIXMULTIPLY_LOCAL  = 5,
	D3DOP_STATETRANSFORM_LOCAL  = 6,
	D3DOP_STATELIGHT_LOCAL      = 7,
	D3DOP_STATERENDER_LOCAL     = 8,
	D3DOP_PROCESSVERTICES_LOCAL = 9,
	D3DOP_TEXTURELOAD_LOCAL     = 10,
	D3DOP_EXIT_LOCAL            = 11,
	D3DOP_BRANCHFORWARD_LOCAL   = 12,
	D3DOP_SPAN_LOCAL            = 13,
	D3DOP_SETSTATUS_LOCAL       = 14,
};

// D3DINSTRUCTION (4 bytes)
#pragma pack(push, 1)
struct LocalD3DInstruction
{
	BYTE bOpcode;     // LocalD3DOpcode 值
	BYTE bSize;       // 每个数据记录的 DWORD 数
	WORD wCount;      // 记录数
};
#pragma pack(pop)

// D3DTRIANGLE (8 bytes = 4 WORD)
#pragma pack(push, 1)
struct LocalD3DTriangle
{
	WORD v1;
	WORD v2;
	WORD v3;
	WORD wFlags;
};
#pragma pack(pop)

// D3DMATRIX (64 bytes = 16 floats, row-major in DX6 SDK)
#pragma pack(push, 1)
struct LocalD3DMatrix
{
	float _11, _12, _13, _14;
	float _21, _22, _23, _24;
	float _31, _32, _33, _34;
	float _41, _42, _43, _44;
};
#pragma pack(pop)

// D3DMATRIXLOAD (8 bytes = 2 DWORD handles)
#pragma pack(push, 1)
struct LocalD3DMatrixLoad
{
	DWORD hDestMatrix;
	DWORD hSrcMatrix;
};
#pragma pack(pop)

// D3DSTATE (8 bytes)
//   union 1: dtstTransformStateType / dlstLightStateType / drstRenderStateType (DWORD)
//   union 2: dwArg[1] / dvArg[1] (DWORD)
#pragma pack(push, 1)
struct LocalD3DState
{
	DWORD stateType;  // 对应 dx6::D3DRENDERSTATETYPE 枚举
	DWORD dwArg;      // 状态值
};
#pragma pack(pop)

// D3DPROCESSVERTICES (16 bytes)
#pragma pack(push, 1)
struct LocalD3DProcessVertices
{
	DWORD dwFlags;
	WORD  wStart;
	WORD  wDest;
	DWORD dwCount;
	DWORD dwReserved;
};
#pragma pack(pop)

// D3DTEXTURELOAD (8 bytes = 2 DWORD handles)
#pragma pack(push, 1)
struct LocalD3DTextureLoad
{
	DWORD hDestTexture;
	DWORD hSrcTexture;
};
#pragma pack(pop)

// 工具：D3DOP_* → 字符串（仅用于日志/调试）
const char* OpcodeName(BYTE op)
{
	switch (static_cast<LocalD3DOpcode>(op))
	{
	case D3DOP_POINT_LOCAL:           return "D3DOP_POINT";
	case D3DOP_LINE_LOCAL:            return "D3DOP_LINE";
	case D3DOP_TRIANGLE_LOCAL:        return "D3DOP_TRIANGLE";
	case D3DOP_MATRIXLOAD_LOCAL:      return "D3DOP_MATRIXLOAD";
	case D3DOP_MATRIXMULTIPLY_LOCAL:  return "D3DOP_MATRIXMULTIPLY";
	case D3DOP_STATETRANSFORM_LOCAL:  return "D3DOP_STATETRANSFORM";
	case D3DOP_STATELIGHT_LOCAL:      return "D3DOP_STATELIGHT";
	case D3DOP_STATERENDER_LOCAL:     return "D3DOP_STATERENDER";
	case D3DOP_PROCESSVERTICES_LOCAL: return "D3DOP_PROCESSVERTICES";
	case D3DOP_TEXTURELOAD_LOCAL:     return "D3DOP_TEXTURELOAD";
	case D3DOP_EXIT_LOCAL:            return "D3DOP_EXIT";
	case D3DOP_BRANCHFORWARD_LOCAL:   return "D3DOP_BRANCHFORWARD";
	case D3DOP_SPAN_LOCAL:            return "D3DOP_SPAN";
	case D3DOP_SETSTATUS_LOCAL:       return "D3DOP_SETSTATUS";
	default:                         return "D3DOP_UNKNOWN";
	}
}

// D3DOP_TRIANGLE (Task 9.3.2)
//
// 数据格式: D3DTRIANGLE[wCount], 每个 D3DTRIANGLE = 4 WORD = 8 bytes
//   所以 bSize 应该是 2 (DWORD 数, 2*4 = 8 bytes per record)
bool ParseTriangle(const LocalD3DInstruction* inst,
	const BYTE* data, ExecuteBufferParser::ParseStats& stats, ExtractedGeometry& out)
{
	// bSize 合法性: D3DTRIANGLE = 8 bytes = 2 DWORDs
	if (inst->bSize != sizeof(LocalD3DTriangle) / 4)
	{
		// 不对齐：跳过（保守），不让 Parse 失败
		stats.unknownOpsSkipped++;
		return true;
	}

	const auto* tris = reinterpret_cast<const LocalD3DTriangle*>(data);
	const size_t count = inst->wCount;

	try
	{
		out.triangles.reserve(out.triangles.size() + count);
	}
	catch (...)
	{
		return false;
	}

	for (size_t i = 0; i < count; ++i)
	{
		ExtractedGeometry::Triangle tri;
		tri.v1 = tris[i].v1;
		tri.v2 = tris[i].v2;
		tri.v3 = tris[i].v3;
		tri.flags = tris[i].wFlags;
		out.triangles.push_back(tri);
		stats.trianglesExtracted++;
	}
	return true;
}

// D3DOP_MATRIXLOAD (Task 9.3.3)
//
// 数据格式（DX6 SDK）：
//   每个 record = D3DMATRIXLOAD (8 bytes) + D3DMATRIX (64 bytes) = 72 bytes = 18 DWORDs
//   D3DMATRIXLOAD { D3DMATRIXHANDLE hDestMatrix; D3DMATRIXHANDLE hSrcMatrix; }
//
// WHY: 真实游戏（如流星蝴蝶剑）会用 MATRIXLOAD 立即设置变换矩阵，
//   必须读矩阵内容才能构建 GBuffer 相机参数。
bool ParseMatrixLoad(const LocalD3DInstruction* inst,
	const BYTE* data, ExecuteBufferParser::ParseStats& stats, ExtractedGeometry& out)
{
	// bSize 合法性: 2 (D3DMATRIXLOAD) + 16 (D3DMATRIX) = 18 DWORDs per record
	const DWORD expected = (sizeof(DWORD) * 2 + sizeof(LocalD3DMatrix)) / 4;
	if (inst->bSize != expected)
	{
		// 退化：bSize == 2 时只读 handle（无矩阵内容），记录为空矩阵
		if (inst->bSize == 2)
		{
			// 只跳过，不提取矩阵内容（占位 stub）
			return true;
		}
		stats.unknownOpsSkipped++;
		return true;
	}

	const size_t count = inst->wCount;
	const size_t recordBytes = inst->bSize * 4u;

	for (size_t i = 0; i < count; ++i)
	{
		const BYTE* rec = data + i * recordBytes;
		// rec[0..7]  = D3DMATRIXLOAD (hDest + hSrc handles)
		// rec[8..71] = D3DMATRIX 16 floats
		const auto* mat = reinterpret_cast<const LocalD3DMatrix*>(rec + sizeof(DWORD) * 2);

		ExtractedGeometry::Matrix4x4 m;
		// 拷贝 4x4 = 16 floats
		m.m[0][0] = mat->_11; m.m[0][1] = mat->_12; m.m[0][2] = mat->_13; m.m[0][3] = mat->_14;
		m.m[1][0] = mat->_21; m.m[1][1] = mat->_22; m.m[1][2] = mat->_23; m.m[1][3] = mat->_24;
		m.m[2][0] = mat->_31; m.m[2][1] = mat->_32; m.m[2][2] = mat->_33; m.m[2][3] = mat->_34;
		m.m[3][0] = mat->_41; m.m[3][1] = mat->_42; m.m[3][2] = mat->_43; m.m[3][3] = mat->_44;

		try
		{
			out.matrices.push_back(m);
		}
		catch (...)
		{
			return false;
		}
		stats.matricesExtracted++;
	}
	return true;
}

// D3DOP_STATERENDER (Task 9.3.5)
//
// 数据格式: D3DSTATE[wCount], 每个 D3DSTATE = 8 bytes
//   = 2 DWORDs per record, bSize == 2
bool ParseStateRender(const LocalD3DInstruction* inst,
	const BYTE* data, ExecuteBufferParser::ParseStats& stats, ExtractedGeometry& out)
{
	// bSize 合法性
	if (inst->bSize != sizeof(LocalD3DState) / 4)
	{
		stats.unknownOpsSkipped++;
		return true;
	}

	const auto* states = reinterpret_cast<const LocalD3DState*>(data);
	const size_t count = inst->wCount;

	try
	{
		out.renderStates.reserve(out.renderStates.size() + count);
	}
	catch (...)
	{
		return false;
	}

	for (size_t i = 0; i < count; ++i)
	{
		ExtractedGeometry::RenderState rs;
		// D3DSTATE 第一个 DWORD 是 state 枚举值, 第二个是值
		rs.state = states[i].stateType;
		rs.value = states[i].dwArg;
		out.renderStates.push_back(rs);
		stats.stateChangesExtracted++;
	}
	return true;
}

// D3DOP_PROCESSVERTICES (Task 9.3.6)
//
// 数据格式: D3DPROCESSVERTICES[wCount], 每个 D3DPROCESSVERTICES = 16 bytes
//   = 4 DWORDs per record, bSize == 4
//
// 注意: D3DOP_PROCESSVERTICES (9) 和 D3DOP_TEXTURELOAD (10) 的 enum 值不同,
//   不可能混淆。任务描述里说"通过上下文区分"是错的, enum 值本就唯一。
bool ParseProcessVertices(const LocalD3DInstruction* inst,
	const BYTE* data, ExecuteBufferParser::ParseStats& stats, ExtractedGeometry& out)
{
	// bSize 合法性
	if (inst->bSize != sizeof(LocalD3DProcessVertices) / 4)
	{
		stats.unknownOpsSkipped++;
		return true;
	}

	const auto* ops = reinterpret_cast<const LocalD3DProcessVertices*>(data);
	const size_t count = inst->wCount;

	try
	{
		out.processVertexOps.reserve(out.processVertexOps.size() + count);
	}
	catch (...)
	{
		return false;
	}

	for (size_t i = 0; i < count; ++i)
	{
		ExtractedGeometry::ProcessVertexOp op;
		op.dwFlags    = ops[i].dwFlags;
		op.wStart     = ops[i].wStart;
		op.wDest      = ops[i].wDest;
		op.dwCount    = ops[i].dwCount;
		op.dwReserved = ops[i].dwReserved;
		out.processVertexOps.push_back(op);
		stats.processVertexOpsExtracted++;
	}
	return true;
}

// D3DOP_TEXTURELOAD (Task 9.3.4)
//
// 数据格式: D3DTEXTURELOAD[wCount], 每个 D3DTEXTURELOAD = 8 bytes
//   = 2 DWORDs per record, bSize == 2
//
// 真实游戏很少用（多走 SetTexture 立即模式），简化为只存 handle
bool ParseTextureLoad(const LocalD3DInstruction* inst,
	const BYTE* data, ExecuteBufferParser::ParseStats& stats, ExtractedGeometry& out)
{
	// bSize 合法性
	if (inst->bSize != sizeof(LocalD3DTextureLoad) / 4)
	{
		stats.unknownOpsSkipped++;
		return true;
	}

	const auto* tex = reinterpret_cast<const LocalD3DTextureLoad*>(data);
	const size_t count = inst->wCount;

	try
	{
		out.textures.reserve(out.textures.size() + count);
	}
	catch (...)
	{
		return false;
	}

	for (size_t i = 0; i < count; ++i)
	{
		ExtractedGeometry::TextureHandle th;
		th.handle = tex[i].hDestTexture;
		out.textures.push_back(th);
		stats.texturesExtracted++;
	}
	return true;
}

// 单条指令分发
bool DispatchInstruction(const LocalD3DInstruction* inst,
	const BYTE* data, ExecuteBufferParser::ParseStats& stats, ExtractedGeometry& out)
{
	switch (static_cast<LocalD3DOpcode>(inst->bOpcode))
	{
	case D3DOP_TRIANGLE_LOCAL:
		return ParseTriangle(inst, data, stats, out);

	case D3DOP_MATRIXLOAD_LOCAL:
		return ParseMatrixLoad(inst, data, stats, out);

	case D3DOP_STATERENDER_LOCAL:
		return ParseStateRender(inst, data, stats, out);

	case D3DOP_PROCESSVERTICES_LOCAL:
		return ParseProcessVertices(inst, data, stats, out);

	case D3DOP_TEXTURELOAD_LOCAL:
		return ParseTextureLoad(inst, data, stats, out);

	// 未实现的 op：跳过 + 计数（不报错，避免游戏不兼容时崩）
	case D3DOP_POINT_LOCAL:
	case D3DOP_LINE_LOCAL:
	case D3DOP_MATRIXMULTIPLY_LOCAL:
	case D3DOP_STATETRANSFORM_LOCAL:
	case D3DOP_STATELIGHT_LOCAL:
	case D3DOP_BRANCHFORWARD_LOCAL:
	case D3DOP_SPAN_LOCAL:
	case D3DOP_SETSTATUS_LOCAL:
		stats.unknownOpsSkipped++;
		return true;

	default:
		// 真正未知的 opcode：保守跳过，避免 buffer 错位时循环
		stats.unknownOpsSkipped++;
		return true;
	}
}

} // anonymous namespace

// -------------------- 构造 / 析构 --------------------
ExecuteBufferParser::ExecuteBufferParser()
	: m_lastError(nullptr)
	, m_vertexData(nullptr)
	, m_vertexDataSize(0)
{
	std::memset(&m_stats, 0, sizeof(m_stats));
}

ExecuteBufferParser::~ExecuteBufferParser() = default;

// -------------------- 主入口 --------------------
bool ExecuteBufferParser::Parse(const void* bufferData, DWORD bufferSize, ExtractedGeometry& out)
{
	// 清空统计 + 错误状态
	std::memset(&m_stats, 0, sizeof(m_stats));
	m_lastError = nullptr;

	if (!bufferData || bufferSize == 0)
	{
		SetError("null buffer or zero size");
		return false;
	}

	// 至少要容纳 1 个 D3DINSTRUCTION (4 bytes)
	if (bufferSize < sizeof(LocalD3DInstruction))
	{
		SetError("buffer too small for D3DINSTRUCTION header");
		return false;
	}

	// DWORD 对齐检查
	if (!IsDwordAligned(bufferData))
	{
		SetError("buffer not DWORD aligned");
		return false;
	}

	// 清空 out（保留容量，零分配）
	out.vertices.clear();
	out.triangles.clear();
	out.matrices.clear();
	out.textures.clear();
	out.renderStates.clear();
	out.processVertexOps.clear();

	const BYTE* p = static_cast<const BYTE*>(bufferData);
	const BYTE* const end = p + bufferSize;

	// 主循环：每条 D3DINSTRUCTION
	while (p < end)
	{
		// 至少 4 字节 (D3DINSTRUCTION header)
		if (static_cast<DWORD>(end - p) < sizeof(LocalD3DInstruction))
		{
			// WHY: 部分 D3D 实现会在 EXIT 之后留 padding 字节，视为 OK 不报错
			//   这里 break（不报错）即可。
			m_stats.bytesConsumed = static_cast<DWORD>(p - static_cast<const BYTE*>(bufferData));
			return true;
		}

		const LocalD3DInstruction* inst = reinterpret_cast<const LocalD3DInstruction*>(p);

		// bSize 是每个数据记录的 DWORD 数（0 非法；EXIT 应该是 bSize=0 + wCount=0）
		// bSize 最大不能超过 buffer 剩余字节 / 4
		const DWORD recordBytes = static_cast<DWORD>(inst->bSize) * 4u;
		const DWORD totalDataBytes = recordBytes * static_cast<DWORD>(inst->wCount);

		// 越界检查
		if (static_cast<DWORD>(end - p) < sizeof(LocalD3DInstruction) + totalDataBytes)
		{
			SetError("instruction data overruns buffer end");
			return false;
		}

		// 数据指针
		const BYTE* dataPtr = p + sizeof(LocalD3DInstruction);

		// EXIT 是终止符，无数据
		if (inst->bOpcode == D3DOP_EXIT_LOCAL)
		{
			// WHY: 严格按 Wine d3d.c 行为，EXIT 指令应立即终止解析，不继续读后面
			//   即使后面还有 bytes，也是被忽略的 padding
			m_stats.instructionsParsed++;
			m_stats.bytesConsumed = static_cast<DWORD>(dataPtr - static_cast<const BYTE*>(bufferData));
			return true;
		}

		// 解析单条指令
		if (!DispatchInstruction(inst, dataPtr, m_stats, out))
		{
			// OOM 之类不可恢复错误
			SetError("instruction dispatch failed (OOM or malformed)");
			return false;
		}

		// 推进指针：header + data
		p = dataPtr + totalDataBytes;
		m_stats.instructionsParsed++;
	}

	// 流到末尾也没遇到 EXIT（不致命，记日志）
	m_stats.bytesConsumed = static_cast<DWORD>(p - static_cast<const BYTE*>(bufferData));
	return true;
}

} // namespace Render
} // namespace NDDFIX
