; example_lightcontrol.as
; AutoHotkey v1 脚本范例
; 演示如何通过 Ddfix API 实时控制游戏内光源 / 环境 / 雾效 / UI
;
; 公开文献参考:
;   - AutoHotkey v1 DllCall 文档:
;     https://www.autohotkey.com/docs/commands/DllCall.htm
;   - AutoHotkey v1 #Include 文档:
;     https://www.autohotkey.com/docs/commands/_Include.htm
;
; 编译 / 加载方式:
;   1. 将 ddfix.dll 注入到目标游戏进程 (例: DLL 注入器 / Extreme Injector)
;   2. 在 AHK 脚本中通过 DllCall 直接调 ddfix.dll 导出的 16 个 Ddfix_* 函数
;   3. 16 个 API 全是 __stdcall 调用约定, 简单类型 (int / float / const char*)
;      → 不需要 marshaling wrapper, 直接 DllCall 即可
;
; 注意:
;   - 字符串返 const char* 是 ddfix 单例内 std::string::c_str(), 永生,
;     但 AHK 端按惯例应立即复制一次使用, 避免跨线程 DllCall 风险.
;   - 所有 float 参数范围: 0.0~1.0 表示归一化值, 数值可超过 1.0 表示
;     增强效果 (例如 Ddfix_SetLightBrightness(2.0) = 2 倍亮度).
;   - 颜色分量 r/g/b 范围 0.0~1.0 (与 D3DCOLORVALUE 规范一致).

#SingleInstance Force
#Persistent
#NoEnv

; -------------------- 启动: 初始化 ddfix 渲染管线 --------------------
; 启用 ddfix 渲染管线 (Phase 9.7 起, 默认就是 enabled, 这里显式调一次便于脚本调试)
Ddfix_SetRenderEnabled(true)

; 选中主光源
Ddfix_SelectLight("MainLight")

; 调整光源位置 (世界坐标)
Ddfix_SetLightPosition(100, 200, 50)

; 调整光源颜色 (暖色: 1.0 红, 0.8 绿, 0.6 蓝)
Ddfix_SetLightColor(1.0, 0.8, 0.6)

; 调整光源亮度 (1.5 倍, 比标准更亮)
Ddfix_SetLightBrightness(1.5)

; 增强阴影 (0.0=无, 1.0=全)
Ddfix_SetLightShadowStrength(0.8)

; 调整环境光 (冷色调)
Ddfix_SetAmbientColor(0.3, 0.3, 0.4)

; 调整雾效
Ddfix_SetFogDistance(50, 800)        ; 雾起始 50 单位, 800 单位处全雾
Ddfix_SetFogColor(0.6, 0.6, 0.7)    ; 雾色: 浅紫灰

; 显示战斗 UI
Ddfix_SetArenaUIVisible(true)

; -------------------- 热键: F1 切换日 / 夜 --------------------
; F1 按下: 切换光源启用状态, 模拟"开灯 / 关灯"
F1::
    if (Ddfix_IsLightEnabled()) {
        Ddfix_SetLightEnabled(false)
        ; 关灯时: 雾色加深, 模拟夜色
        Ddfix_SetLightColor(0.1, 0.1, 0.2)
        Ddfix_SetAmbientColor(0.05, 0.05, 0.1)
        ToolTip, 光源已关闭 (夜色模式)
    } else {
        Ddfix_SetLightEnabled(true)
        ; 开灯时: 恢复暖色
        Ddfix_SetLightColor(1.0, 0.8, 0.6)
        Ddfix_SetAmbientColor(0.3, 0.3, 0.4)
        ToolTip, 光源已开启 (日间模式)
    }
    SetTimer, RemoveToolTip, 1000
    return

RemoveToolTip:
    ToolTip
    SetTimer, RemoveToolTip, Off
    return

; -------------------- 热键: F2 读取当前状态 --------------------
; F2 按下: 弹窗显示当前 ddfix 状态 (供调试用)
F2::
    renderEnabled := Ddfix_IsRenderEnabled()
    lightEnabled  := Ddfix_IsLightEnabled()
    arenaVisible  := Ddfix_IsArenaUIVisible()
    brightness    := Ddfix_GetLightBrightness()
    currentCfg    := Ddfix_GetCurrentRenderConfig()
    selectedLight := Ddfix_GetSelectedLight()

    ; 取光位置 + 颜色 (3 个 float 通过 VarSetCapacity 提取)
    VarSetCapacity(lightPosX, 4, 0)
    VarSetCapacity(lightPosY, 4, 0)
    VarSetCapacity(lightPosZ, 4, 0)
    Ddfix_GetLightPosition(&lightPosX, &lightPosY, &lightPosZ)

    VarSetCapacity(lightColR, 4, 0)
    VarSetCapacity(lightColG, 4, 0)
    VarSetCapacity(lightColB, 4, 0)
    Ddfix_GetLightColor(&lightColR, &lightColG, &lightColB)

    ; 把 4 字节二进制按 IEEE 754 解析成 float
    lpx := NumGet(lightPosX, 0, "Float")
    lpy := NumGet(lightPosY, 0, "Float")
    lpz := NumGet(lightPosZ, 0, "Float")
    lcr := NumGet(lightColR, 0, "Float")
    lcg := NumGet(lightColG, 0, "Float")
    lcb := NumGet(lightColB, 0, "Float")

    MsgBox, 4096, ddfix 状态,
    ( LTrim
    渲染管线:    %renderEnabled% (1=enabled, 0=disabled)
    光源启用:    %lightEnabled%
    ArenaUI:    %arenaVisible%
    光源亮度:    %brightness%
    当前 config:  %currentCfg%
    选中光源:    %selectedLight%
    光源位置:    (%lpx%, %lpy%, %lpz%)
    光源颜色:    (%lcr%, %lcg%, %lcb%)
    )
    return

; -------------------- 热键: F3 切换 render config --------------------
; F3 按下: 在 "Default" 和 "Indoor" 之间切换 render config section
F3::
    cfg := Ddfix_GetCurrentRenderConfig()
    if (cfg = "Default") {
        Ddfix_ChangeRenderConfig("Indoor")
        ToolTip, 已切到 Indoor 配置
    } else {
        Ddfix_ChangeRenderConfig("Default")
        ToolTip, 已切到 Default 配置
    }
    SetTimer, RemoveToolTip, 1000
    return

; -------------------- 热键: F4 切换 Arena UI --------------------
; F4 按下: 切换战斗 UI 显隐
F4::
    if (Ddfix_IsArenaUIVisible()) {
        Ddfix_SetArenaUIVisible(false)
        ToolTip, Arena UI 已隐藏
    } else {
        Ddfix_SetArenaUIVisible(true)
        ToolTip, Arena UI 已显示
    }
    SetTimer, RemoveToolTip, 1000
    return

; -------------------- 热键: ESC 退出 --------------------
; ESC 按下: 退出脚本
Esc::
    ; 退出前清理状态
    Ddfix_SetLightEnabled(true)
    Ddfix_SetLightColor(1.0, 1.0, 1.0)
    Ddfix_SetAmbientColor(0.2, 0.2, 0.2)
    Ddfix_SetLightShadowStrength(0.5)
    ExitApp
    return
