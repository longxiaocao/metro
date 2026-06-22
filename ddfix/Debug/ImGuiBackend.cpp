// Phase 4.1: ImGuiBackend 桩实现
// 全部委托给 HudRenderer（Phase 4.2）。详见 ImGuiBackend.h 头注释。

#include "ImGuiBackend.h"

namespace NDDFIX
{
namespace Debug
{

ImGuiBackend* ImGuiBackend::Instance()
{
	static ImGuiBackend inst;
	return &inst;
}

} // namespace Debug
} // namespace NDDFIX
