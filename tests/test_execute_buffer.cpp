// test_execute_buffer.cpp - Phase 9.3 ExecuteBuffer 解析器单元测试
//
// 覆盖范围 (Task 9.3.13):
//   1. 边界条件: null buffer / zero size / 越界 / 非 DWORD 对齐
//   2. D3DOP_TRIANGLE 解析 (Task 9.3.2)
//   3. D3DOP_MATRIXLOAD 解析 (Task 9.3.3)
//   4. D3DOP_STATERENDER 解析 (Task 9.3.5)
//   5. D3DOP_PROCESSVERTICES 解析 (Task 9.3.6)
//   6. D3DOP_TEXTURELOAD 解析 (Task 9.3.4)
//   7. D3DOP_EXIT 终止
//   8. 未知 / 未实现 opcode 优雅跳过
//   9. ParseStats 计数正确性
//  10. 多指令流混合流
//
// 不覆盖:
//   - IDirect3DDevice::Execute 集成路径 (需真 D3D 设备 + ExecuteBuffer mock, 在 ddfixtests 范围外)
//   - 实际 GBuffer 渲染 (Phase 9.3.8+)

#include "SingleTest.h"

#include "../ddfix/ddraw/ExecuteBufferParser.h"

#include <cstring>
#include <vector>

namespace
{

using NDDFIX::Render::ExecuteBufferParser;
using NDDFIX::Render::ExtractedGeometry;

// -------------------- D3D 指令流构造工具 --------------------
//
// 所有指令 / 数据按 4 字节 DWORD 对齐. 真实 DirectX 6/7 SDK 公开格式:
//   D3DINSTRUCTION { BYTE bOpcode; BYTE bSize; WORD wCount; }
//   <wCount records of bSize DWORDs each>
//
// 测试中直接构造字节 buffer, 喂给 parser.Parse().

// 包成 POD struct (1-byte pack) 避免对齐填充.
#pragma pack(push, 1)
struct TestInstruction
{
	BYTE bOpcode;
	BYTE bSize;
	WORD wCount;
};
struct TestTriangle
{
	WORD v1, v2, v3, wFlags;
};
struct TestMatrixLoad
{
	DWORD hDest;
	DWORD hSrc;
};
struct TestMatrix
{
	float _11, _12, _13, _14;
	float _21, _22, _23, _24;
	float _31, _32, _33, _34;
	float _41, _42, _43, _44;
};
struct TestMatrixLoadRecord
{
	TestMatrixLoad handles;
	TestMatrix       mat;
};
struct TestState
{
	DWORD stateType;
	DWORD value;
};
struct TestProcessVertices
{
	DWORD dwFlags;
	WORD  wStart;
	WORD  wDest;
	DWORD dwCount;
	DWORD dwReserved;
};
struct TestTextureLoad
{
	DWORD hDest;
	DWORD hSrc;
};
#pragma pack(pop)

// 追加 D3DOP_TRIANGLE 指令到 buffer
void AppendTriangle(std::vector<BYTE>& buf, const std::vector<TestTriangle>& tris)
{
	TestInstruction hdr;
	hdr.bOpcode = 3;          // D3DOP_TRIANGLE
	hdr.bSize   = sizeof(TestTriangle) / 4;  // 8/4 = 2 DWORDs
	hdr.wCount  = static_cast<WORD>(tris.size());
	const BYTE* hdrBytes = reinterpret_cast<const BYTE*>(&hdr);
	buf.insert(buf.end(), hdrBytes, hdrBytes + sizeof(hdr));
	for (const auto& t : tris)
	{
		const BYTE* bytes = reinterpret_cast<const BYTE*>(&t);
		buf.insert(buf.end(), bytes, bytes + sizeof(t));
	}
}

// 追加 D3DOP_MATRIXLOAD 指令到 buffer
void AppendMatrixLoad(std::vector<BYTE>& buf, const std::vector<TestMatrixLoadRecord>& recs)
{
	TestInstruction hdr;
	hdr.bOpcode = 4;          // D3DOP_MATRIXLOAD
	hdr.bSize   = sizeof(TestMatrixLoadRecord) / 4;  // 72/4 = 18 DWORDs
	hdr.wCount  = static_cast<WORD>(recs.size());
	const BYTE* hdrBytes = reinterpret_cast<const BYTE*>(&hdr);
	buf.insert(buf.end(), hdrBytes, hdrBytes + sizeof(hdr));
	for (const auto& r : recs)
	{
		const BYTE* bytes = reinterpret_cast<const BYTE*>(&r);
		buf.insert(buf.end(), bytes, bytes + sizeof(r));
	}
}

// 追加 D3DOP_STATERENDER 指令
void AppendStateRender(std::vector<BYTE>& buf, const std::vector<TestState>& states)
{
	TestInstruction hdr;
	hdr.bOpcode = 8;          // D3DOP_STATERENDER
	hdr.bSize   = sizeof(TestState) / 4;  // 8/4 = 2 DWORDs
	hdr.wCount  = static_cast<WORD>(states.size());
	const BYTE* hdrBytes = reinterpret_cast<const BYTE*>(&hdr);
	buf.insert(buf.end(), hdrBytes, hdrBytes + sizeof(hdr));
	for (const auto& s : states)
	{
		const BYTE* bytes = reinterpret_cast<const BYTE*>(&s);
		buf.insert(buf.end(), bytes, bytes + sizeof(s));
	}
}

// 追加 D3DOP_PROCESSVERTICES 指令
void AppendProcessVertices(std::vector<BYTE>& buf, const std::vector<TestProcessVertices>& ops)
{
	TestInstruction hdr;
	hdr.bOpcode = 9;          // D3DOP_PROCESSVERTICES
	hdr.bSize   = sizeof(TestProcessVertices) / 4;  // 16/4 = 4 DWORDs
	hdr.wCount  = static_cast<WORD>(ops.size());
	const BYTE* hdrBytes = reinterpret_cast<const BYTE*>(&hdr);
	buf.insert(buf.end(), hdrBytes, hdrBytes + sizeof(hdr));
	for (const auto& o : ops)
	{
		const BYTE* bytes = reinterpret_cast<const BYTE*>(&o);
		buf.insert(buf.end(), bytes, bytes + sizeof(o));
	}
}

// 追加 D3DOP_TEXTURELOAD 指令
void AppendTextureLoad(std::vector<BYTE>& buf, const std::vector<TestTextureLoad>& texs)
{
	TestInstruction hdr;
	hdr.bOpcode = 10;         // D3DOP_TEXTURELOAD
	hdr.bSize   = sizeof(TestTextureLoad) / 4;  // 8/4 = 2 DWORDs
	hdr.wCount  = static_cast<WORD>(texs.size());
	const BYTE* hdrBytes = reinterpret_cast<const BYTE*>(&hdr);
	buf.insert(buf.end(), hdrBytes, hdrBytes + sizeof(hdr));
	for (const auto& t : texs)
	{
		const BYTE* bytes = reinterpret_cast<const BYTE*>(&t);
		buf.insert(buf.end(), bytes, bytes + sizeof(t));
	}
}

// 追加 D3DOP_EXIT 指令 (bSize=0, wCount=0)
void AppendExit(std::vector<BYTE>& buf)
{
	TestInstruction hdr;
	hdr.bOpcode = 11;         // D3DOP_EXIT
	hdr.bSize   = 0;
	hdr.wCount  = 0;
	const BYTE* hdrBytes = reinterpret_cast<const BYTE*>(&hdr);
	buf.insert(buf.end(), hdrBytes, hdrBytes + sizeof(hdr));
}

} // anonymous namespace

// ============================================================
// 1. 边界条件
// ============================================================
TEST(ExecuteBufferParser, NullBuffer)
{
	ExecuteBufferParser parser;
	ExtractedGeometry out;
	EXPECT_FALSE(parser.Parse(nullptr, 100, out));
	EXPECT_TRUE(parser.GetLastError() != nullptr);
}

TEST(ExecuteBufferParser, ZeroSize)
{
	ExecuteBufferParser parser;
	ExtractedGeometry out;
	DWORD dummy = 0;
	EXPECT_FALSE(parser.Parse(&dummy, 0, out));
	EXPECT_TRUE(parser.GetLastError() != nullptr);
}

TEST(ExecuteBufferParser, TooSmallForHeader)
{
	ExecuteBufferParser parser;
	ExtractedGeometry out;
	BYTE buf[3] = { 0, 0, 0 };  // < 4 字节, 容不下 D3DINSTRUCTION
	EXPECT_FALSE(parser.Parse(buf, 3, out));
	EXPECT_TRUE(parser.GetLastError() != nullptr);
}

TEST(ExecuteBufferParser, UnalignedBuffer)
{
	// 故意给一个非 4 字节对齐的指针
	BYTE  raw[64] = {};
	BYTE* p = raw + 1;  // 偏移 1 字节, 故意不 DWORD 对齐
	ExecuteBufferParser parser;
	ExtractedGeometry out;
	EXPECT_FALSE(parser.Parse(p, sizeof(raw) - 1, out));
	EXPECT_TRUE(parser.GetLastError() != nullptr);
}

TEST(ExecuteBufferParser, InstructionDataOverrunsEnd)
{
	// bSize 合法但总数据越界: 1 条 triangle (需要 2 DWORDs) 但 buffer 只剩 header
	BYTE buf[4] = { 3, 2, 1, 0 };  // D3DOP_TRIANGLE, bSize=2, wCount=1, 但 buffer 已结束
	ExecuteBufferParser parser;
	ExtractedGeometry out;
	EXPECT_FALSE(parser.Parse(buf, sizeof(buf), out));
	EXPECT_TRUE(parser.GetLastError() != nullptr);
}

// ============================================================
// 2. D3DOP_TRIANGLE 解析 (Task 9.3.2)
// ============================================================
TEST(ExecuteBufferParser, TriangleSingle)
{
	std::vector<BYTE> buf;
	AppendTriangle(buf, { {0, 1, 2, 0} });
	AppendExit(buf);

	ExecuteBufferParser parser;
	ExtractedGeometry out;
	EXPECT_TRUE(parser.Parse(buf.data(), static_cast<DWORD>(buf.size()), out));
	EXPECT_EQ(out.triangles.size(), static_cast<size_t>(1));
	if (out.triangles.size() == 1)
	{
		EXPECT_EQ(out.triangles[0].v1, 0u);
		EXPECT_EQ(out.triangles[0].v2, 1u);
		EXPECT_EQ(out.triangles[0].v3, 2u);
		EXPECT_EQ(out.triangles[0].flags, 0u);
	}
	EXPECT_EQ(parser.GetStats().trianglesExtracted, 1u);
	EXPECT_EQ(parser.GetStats().instructionsParsed, 2u);  // TRIANGLE + EXIT
}

TEST(ExecuteBufferParser, TriangleMultiple)
{
	std::vector<BYTE> buf;
	AppendTriangle(buf, {
		{0, 1, 2, 0x0001u},
		{2, 3, 0, 0x0002u},
		{0, 3, 1, 0x0003u},
	});
	AppendExit(buf);

	ExecuteBufferParser parser;
	ExtractedGeometry out;
	EXPECT_TRUE(parser.Parse(buf.data(), static_cast<DWORD>(buf.size()), out));
	EXPECT_EQ(out.triangles.size(), static_cast<size_t>(3));
	EXPECT_EQ(parser.GetStats().trianglesExtracted, 3u);
	if (out.triangles.size() == 3)
	{
		EXPECT_EQ(out.triangles[2].flags, 0x0003u);
	}
}

TEST(ExecuteBufferParser, TriangleBadSizeSkipped)
{
	// bSize != 2: 应当被跳过 (unknownOpsSkipped++), 但不阻塞后续指令
	std::vector<BYTE> buf;
	TestInstruction badHdr = { 3, 0, 1 };  // D3DOP_TRIANGLE, bSize=0 (非法)
	const BYTE* badHdrBytes = reinterpret_cast<const BYTE*>(&badHdr);
	buf.insert(buf.end(), badHdrBytes, badHdrBytes + sizeof(badHdr));
	// 0 数据字节 (bSize=0), 不占空间
	AppendTriangle(buf, { {0, 1, 2, 0} });  // 1 个有效 triangle
	AppendExit(buf);

	ExecuteBufferParser parser;
	ExtractedGeometry out;
	EXPECT_TRUE(parser.Parse(buf.data(), static_cast<DWORD>(buf.size()), out));
	EXPECT_EQ(out.triangles.size(), static_cast<size_t>(1));
	EXPECT_TRUE(parser.GetStats().unknownOpsSkipped >= 1u);
}

// ============================================================
// 3. D3DOP_MATRIXLOAD 解析 (Task 9.3.3)
// ============================================================
TEST(ExecuteBufferParser, MatrixLoad)
{
	std::vector<BYTE> buf;
	TestMatrixLoadRecord rec = {};
	rec.handles.hDest = 0x1000;
	rec.handles.hSrc  = 0x2000;
	// 单位矩阵
	rec.mat._11 = 1.0f; rec.mat._22 = 1.0f; rec.mat._33 = 1.0f; rec.mat._44 = 1.0f;
	AppendMatrixLoad(buf, { rec });
	AppendExit(buf);

	ExecuteBufferParser parser;
	ExtractedGeometry out;
	EXPECT_TRUE(parser.Parse(buf.data(), static_cast<DWORD>(buf.size()), out));
	EXPECT_EQ(out.matrices.size(), static_cast<size_t>(1));
	if (out.matrices.size() == 1)
	{
		EXPECT_EQ(out.matrices[0].m[0][0], 1.0f);
		EXPECT_EQ(out.matrices[0].m[1][1], 1.0f);
		EXPECT_EQ(out.matrices[0].m[2][2], 1.0f);
		EXPECT_EQ(out.matrices[0].m[3][3], 1.0f);
	}
	EXPECT_EQ(parser.GetStats().matricesExtracted, 1u);
}

TEST(ExecuteBufferParser, MatrixLoadMultiple)
{
	std::vector<BYTE> buf;
	std::vector<TestMatrixLoadRecord> recs;
	for (int i = 0; i < 4; ++i)
	{
		TestMatrixLoadRecord r = {};
		r.handles.hDest = 0x1000u + i;
		r.handles.hSrc  = 0x2000u + i;
		// 对角线元素赋不同值, 验证每条独立读
		r.mat._11 = static_cast<float>(i + 1);
		r.mat._22 = static_cast<float>(i + 1);
		r.mat._33 = static_cast<float>(i + 1);
		r.mat._44 = 1.0f;
		recs.push_back(r);
	}
	AppendMatrixLoad(buf, recs);
	AppendExit(buf);

	ExecuteBufferParser parser;
	ExtractedGeometry out;
	EXPECT_TRUE(parser.Parse(buf.data(), static_cast<DWORD>(buf.size()), out));
	EXPECT_EQ(out.matrices.size(), static_cast<size_t>(4));
	if (out.matrices.size() == 4)
	{
		EXPECT_EQ(out.matrices[0].m[0][0], 1.0f);
		EXPECT_EQ(out.matrices[3].m[0][0], 4.0f);
	}
	EXPECT_EQ(parser.GetStats().matricesExtracted, 4u);
}

// ============================================================
// 4. D3DOP_STATERENDER 解析 (Task 9.3.5)
// ============================================================
TEST(ExecuteBufferParser, StateRender)
{
	std::vector<BYTE> buf;
	AppendStateRender(buf, {
		{7, 1},     // D3DRENDERSTATE_ZENABLE = TRUE
		{14, 0xFF}, // D3DRENDERSTATE_FILLMODE
		{27, 0},    // D3DRENDERSTATE_CULLMODE
	});
	AppendExit(buf);

	ExecuteBufferParser parser;
	ExtractedGeometry out;
	EXPECT_TRUE(parser.Parse(buf.data(), static_cast<DWORD>(buf.size()), out));
	EXPECT_EQ(out.renderStates.size(), static_cast<size_t>(3));
	if (out.renderStates.size() == 3)
	{
		EXPECT_EQ(out.renderStates[0].state, 7u);
		EXPECT_EQ(out.renderStates[0].value, 1u);
		EXPECT_EQ(out.renderStates[1].state, 14u);
		EXPECT_EQ(out.renderStates[1].value, 0xFFu);
	}
	EXPECT_EQ(parser.GetStats().stateChangesExtracted, 3u);
}

// ============================================================
// 5. D3DOP_PROCESSVERTICES 解析 (Task 9.3.6)
// ============================================================
TEST(ExecuteBufferParser, ProcessVertices)
{
	std::vector<BYTE> buf;
	AppendProcessVertices(buf, {
		{0x04, 0, 100, 50, 0},  // dwFlags=TRANSFORM, wStart=0, wDest=100, dwCount=50
		{0x10, 0, 0, 200, 0},    // dwFlags=LIGHT, wStart=0, wDest=0, dwCount=200
	});
	AppendExit(buf);

	ExecuteBufferParser parser;
	ExtractedGeometry out;
	EXPECT_TRUE(parser.Parse(buf.data(), static_cast<DWORD>(buf.size()), out));
	EXPECT_EQ(out.processVertexOps.size(), static_cast<size_t>(2));
	if (out.processVertexOps.size() == 2)
	{
		EXPECT_EQ(out.processVertexOps[0].dwFlags, 0x04u);
		EXPECT_EQ(out.processVertexOps[0].wStart, 0u);
		EXPECT_EQ(out.processVertexOps[0].wDest, 100u);
		EXPECT_EQ(out.processVertexOps[0].dwCount, 50u);
	}
	EXPECT_EQ(parser.GetStats().processVertexOpsExtracted, 2u);
}

// ============================================================
// 6. D3DOP_TEXTURELOAD 解析 (Task 9.3.4)
// ============================================================
TEST(ExecuteBufferParser, TextureLoad)
{
	std::vector<BYTE> buf;
	AppendTextureLoad(buf, {
		{0xAA00, 0xBB00},
		{0xCC00, 0xDD00},
	});
	AppendExit(buf);

	ExecuteBufferParser parser;
	ExtractedGeometry out;
	EXPECT_TRUE(parser.Parse(buf.data(), static_cast<DWORD>(buf.size()), out));
	EXPECT_EQ(out.textures.size(), static_cast<size_t>(2));
	if (out.textures.size() == 2)
	{
		EXPECT_EQ(out.textures[0].handle, 0xAA00u);
		EXPECT_EQ(out.textures[1].handle, 0xCC00u);
	}
	EXPECT_EQ(parser.GetStats().texturesExtracted, 2u);
}

// ============================================================
// 7. D3DOP_EXIT 终止
// ============================================================
TEST(ExecuteBufferParser, ExitTerminatesEarly)
{
	std::vector<BYTE> buf;
	AppendTriangle(buf, { {0, 1, 2, 0} });
	AppendExit(buf);
	// EXIT 之后再追加垃圾字节, parser 不应继续解析
	AppendTriangle(buf, { {5, 6, 7, 0} });
	// 多写 16 字节 padding
	for (int i = 0; i < 16; ++i) buf.push_back(0xFF);

	ExecuteBufferParser parser;
	ExtractedGeometry out;
	EXPECT_TRUE(parser.Parse(buf.data(), static_cast<DWORD>(buf.size()), out));
	// 应该只有 EXIT 之前的 1 个 triangle, 后面的被忽略
	EXPECT_EQ(out.triangles.size(), static_cast<size_t>(1));
	EXPECT_EQ(parser.GetStats().instructionsParsed, 2u);  // TRIANGLE + EXIT
}

TEST(ExecuteBufferParser, ExitOnly)
{
	// 只有 EXIT, 立即终止
	std::vector<BYTE> buf;
	AppendExit(buf);

	ExecuteBufferParser parser;
	ExtractedGeometry out;
	EXPECT_TRUE(parser.Parse(buf.data(), static_cast<DWORD>(buf.size()), out));
	EXPECT_EQ(out.triangles.size(), static_cast<size_t>(0));
	EXPECT_EQ(out.matrices.size(), static_cast<size_t>(0));
	EXPECT_EQ(out.renderStates.size(), static_cast<size_t>(0));
	EXPECT_EQ(parser.GetStats().instructionsParsed, 1u);
}

TEST(ExecuteBufferParser, NoExitReachesEnd)
{
	// 没有 EXIT, 解析到 buffer 末尾视为完成 (不报错)
	std::vector<BYTE> buf;
	AppendTriangle(buf, { {0, 1, 2, 0} });
	// 不加 EXIT

	ExecuteBufferParser parser;
	ExtractedGeometry out;
	EXPECT_TRUE(parser.Parse(buf.data(), static_cast<DWORD>(buf.size()), out));
	EXPECT_EQ(out.triangles.size(), static_cast<size_t>(1));
	EXPECT_EQ(parser.GetStats().instructionsParsed, 1u);
}

// ============================================================
// 8. 未知 / 未实现 opcode 跳过
// ============================================================
TEST(ExecuteBufferParser, UnknownOpcodeSkipped)
{
	std::vector<BYTE> buf;
	// D3DOP_POINT (1) 是未实现 op, 应当跳过
	{
		TestInstruction hdr = { 1, 0, 0 };  // POINT, bSize=0, wCount=0
		const BYTE* bytes = reinterpret_cast<const BYTE*>(&hdr);
		buf.insert(buf.end(), bytes, bytes + sizeof(hdr));
	}
	// 1 个有效 triangle
	AppendTriangle(buf, { {0, 1, 2, 0} });
	// D3DOP_LINE (2) 也是未实现
	{
		TestInstruction hdr = { 2, 0, 0 };
		const BYTE* bytes = reinterpret_cast<const BYTE*>(&hdr);
		buf.insert(buf.end(), bytes, bytes + sizeof(hdr));
	}
	AppendExit(buf);

	ExecuteBufferParser parser;
	ExtractedGeometry out;
	EXPECT_TRUE(parser.Parse(buf.data(), static_cast<DWORD>(buf.size()), out));
	EXPECT_EQ(out.triangles.size(), static_cast<size_t>(1));
	EXPECT_TRUE(parser.GetStats().unknownOpsSkipped >= 2u);
}

TEST(ExecuteBufferParser, TrulyUnknownOpcode)
{
	// opcode = 0x80, 完全不在 D3DOP_* 范围, 应当被 default 分支跳过
	std::vector<BYTE> buf;
	{
		TestInstruction hdr = { 0x80, 0, 0 };
		const BYTE* bytes = reinterpret_cast<const BYTE*>(&hdr);
		buf.insert(buf.end(), bytes, bytes + sizeof(hdr));
	}
	AppendTriangle(buf, { {0, 1, 2, 0} });
	AppendExit(buf);

	ExecuteBufferParser parser;
	ExtractedGeometry out;
	EXPECT_TRUE(parser.Parse(buf.data(), static_cast<DWORD>(buf.size()), out));
	EXPECT_EQ(out.triangles.size(), static_cast<size_t>(1));
	EXPECT_TRUE(parser.GetStats().unknownOpsSkipped >= 1u);
}

// ============================================================
// 9. ParseStats 计数正确性
// ============================================================
TEST(ExecuteBufferParser, StatsResetBetweenCalls)
{
	ExecuteBufferParser parser;
	ExtractedGeometry out;
	std::vector<BYTE> buf1;
	AppendTriangle(buf1, { {0, 1, 2, 0} });
	AppendExit(buf1);
	EXPECT_TRUE(parser.Parse(buf1.data(), static_cast<DWORD>(buf1.size()), out));
	EXPECT_EQ(parser.GetStats().trianglesExtracted, 1u);

	// 第二次调用应重置 stats
	std::vector<BYTE> buf2;
	AppendTriangle(buf2, { {0, 1, 2, 0} });
	AppendTriangle(buf2, { {2, 3, 0, 0} });
	AppendExit(buf2);
	EXPECT_TRUE(parser.Parse(buf2.data(), static_cast<DWORD>(buf2.size()), out));
	EXPECT_EQ(parser.GetStats().trianglesExtracted, 2u);  // 1 而非 3
}

TEST(ExecuteBufferParser, BytesConsumed)
{
	std::vector<BYTE> buf;
	AppendTriangle(buf, { {0, 1, 2, 0} });  // 4 + 8 = 12 bytes
	AppendExit(buf);                          // 4 bytes
	EXPECT_EQ(buf.size(), static_cast<size_t>(16));

	ExecuteBufferParser parser;
	ExtractedGeometry out;
	EXPECT_TRUE(parser.Parse(buf.data(), static_cast<DWORD>(buf.size()), out));
	EXPECT_EQ(parser.GetStats().bytesConsumed, 16u);
}

// ============================================================
// 10. 多指令流混合
// ============================================================
TEST(ExecuteBufferParser, MixedInstructions)
{
	// 模拟真实 ExecuteBuffer: STATERENDER + MATRIXLOAD + TRIANGLE + EXIT
	std::vector<BYTE> buf;
	AppendStateRender(buf, { {7, 1} });  // ZENABLE
	TestMatrixLoadRecord mlr = {};
	mlr.handles.hDest = 0x1000;
	mlr.handles.hSrc  = 0x2000;
	mlr.mat._11 = 1.0f; mlr.mat._22 = 1.0f; mlr.mat._33 = 1.0f; mlr.mat._44 = 1.0f;
	AppendMatrixLoad(buf, { mlr });
	AppendTriangle(buf, { {0, 1, 2, 0}, {2, 3, 0, 0} });
	AppendProcessVertices(buf, { {0x04, 0, 0, 4, 0} });
	AppendTextureLoad(buf, { {0xAA, 0xBB} });
	AppendExit(buf);

	ExecuteBufferParser parser;
	ExtractedGeometry out;
	EXPECT_TRUE(parser.Parse(buf.data(), static_cast<DWORD>(buf.size()), out));

	EXPECT_EQ(out.renderStates.size(),   static_cast<size_t>(1));
	EXPECT_EQ(out.matrices.size(),       static_cast<size_t>(1));
	EXPECT_EQ(out.triangles.size(),      static_cast<size_t>(2));
	EXPECT_EQ(out.processVertexOps.size(), static_cast<size_t>(1));
	EXPECT_EQ(out.textures.size(),       static_cast<size_t>(1));

	const auto& s = parser.GetStats();
	EXPECT_EQ(s.trianglesExtracted,         2u);
	EXPECT_EQ(s.matricesExtracted,          1u);
	EXPECT_EQ(s.stateChangesExtracted,      1u);
	EXPECT_EQ(s.processVertexOpsExtracted,  1u);
	EXPECT_EQ(s.texturesExtracted,          1u);
	EXPECT_EQ(s.instructionsParsed,         6u);  // STATE + MATRIX + TRIANGLE + PROC + TEX + EXIT
	EXPECT_EQ(s.unknownOpsSkipped,          0u);
}

TEST(ExecuteBufferParser, OutClearedOnNewParse)
{
	// 第一次填充, 第二次 Parse 应清空 out
	ExecuteBufferParser parser;
	ExtractedGeometry out;

	std::vector<BYTE> buf1;
	AppendTriangle(buf1, { {0, 1, 2, 0} });
	AppendExit(buf1);
	EXPECT_TRUE(parser.Parse(buf1.data(), static_cast<DWORD>(buf1.size()), out));
	EXPECT_EQ(out.triangles.size(), static_cast<size_t>(1));

	// 第二次: 没有 triangle, 只有 state
	std::vector<BYTE> buf2;
	AppendStateRender(buf2, { {7, 1} });
	AppendExit(buf2);
	EXPECT_TRUE(parser.Parse(buf2.data(), static_cast<DWORD>(buf2.size()), out));
	EXPECT_EQ(out.triangles.size(), static_cast<size_t>(0));  // 被清空
	EXPECT_EQ(out.renderStates.size(), static_cast<size_t>(1));
}

// ============================================================
// 11. ExtractedGeometry 默认状态
// ============================================================
TEST(ExecuteBufferParser, DefaultExtractedGeometryEmpty)
{
	ExtractedGeometry out;
	EXPECT_EQ(out.vertices.size(),          static_cast<size_t>(0));
	EXPECT_EQ(out.triangles.size(),         static_cast<size_t>(0));
	EXPECT_EQ(out.matrices.size(),          static_cast<size_t>(0));
	EXPECT_EQ(out.textures.size(),          static_cast<size_t>(0));
	EXPECT_EQ(out.renderStates.size(),      static_cast<size_t>(0));
	EXPECT_EQ(out.processVertexOps.size(),  static_cast<size_t>(0));
}
