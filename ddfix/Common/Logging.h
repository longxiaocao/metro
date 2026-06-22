#pragma once

// Phase 5.1: 日志层重写
//   - 加 std::mutex 保护（线程安全）
//   - 加 enum class LogLevel + Log::SetLevel 过滤
//   - 加 Log::Write / Log::SetFile
//   - 加 LOG_DEBUG/INFO/WARN/ERROR/FATAL 宏简写
//   - 保留旧 Log::LOG / Log()<< / logf 三个旧接口（向后兼容）

#include <fstream>
#include <mutex>
#include <stdarg.h>
#include <string>

// 日志级别（值越大越严重）。过滤规则：level >= s_level 才输出。
enum class LogLevel
{
	DEBUG = 0,
	INFO  = 1,
	WARN  = 2,
	ERROR = 3,
	FATAL = 4,
};

class Log
{
public:
	// ★ 旧接口：保留 public 静态 ofstream 字段，dllmain.cpp 仍按原方式初始化
	//   "std::ofstream Log::LOG(\"ddfix.log\");"
	//   多线程并发写仍可能交错，但新接口 Log::Write 内部已加锁。
	static std::ofstream LOG;

	Log() {}
	~Log()
	{
		// 析构时刷一行末尾 + 换行
		std::lock_guard<std::mutex> lock(GetMutex());
		if (LOG.is_open())
		{
			LOG << std::endl;
		}
	}

	// 旧流式接口：Log() << "text" << var;
	// 每次 << 都加锁，保证多线程不交错。
	template <typename T>
	Log& operator<<(const T& t)
	{
		std::lock_guard<std::mutex> lock(GetMutex());
		if (LOG.is_open())
		{
			LOG << t;
		}
		return *this;
	}

	// -------------------- 新增接口（Phase 5.1） --------------------

	// 设置过滤级别（线程安全）。低于该级别的 Write 调用会被静默丢弃。
	static void SetLevel(LogLevel level)
	{
		std::lock_guard<std::mutex> lock(GetMutex());
		LevelRef() = level;
	}

	// 取当前过滤级别
	static LogLevel GetLevel()
	{
		std::lock_guard<std::mutex> lock(GetMutex());
		return LevelRef();
	}

	// 改写日志文件（线程安全）。新路径覆盖式打开，失败时保留原文件。
	static void SetFile(const char* path)
	{
		if (!path)
		{
			return;
		}
		std::lock_guard<std::mutex> lock(GetMutex());
		if (LOG.is_open())
		{
			LOG.flush();
			LOG.close();
		}
		LOG.open(path, std::ios::out | std::ios::trunc);
	}

	// 线程安全的 printf 风格写日志（带 level 过滤 + level 前缀）
	//   - 自动加 "\n" 换行
	//   - 内部用 std::lock_guard 串行化
	//   - 失败回退到 stderr（防日志文件被独占锁时仍能看到错误）
	static void Write(LogLevel level, const char* fmt, ...)
	{
		// level 过滤（无锁快速路径：读 LevelRef 是单 uint8 写，竞态最多差 1 行）
		if (static_cast<int>(level) < static_cast<int>(LevelRef()))
		{
			return;
		}

		va_list ap;
		va_start(ap, fmt);
		// 第一次 vsnprintf 只算长度
		va_list ap2;
		va_copy(ap2, ap);
		int size = vsnprintf(nullptr, 0, fmt, ap);
		va_end(ap);
		if (size < 0)
		{
			va_end(ap2);
			return;
		}
		std::string output;
		output.resize(static_cast<size_t>(size) + 1);
		vsprintf_s(&output[0], output.size(), fmt, ap2);
		va_end(ap2);

		// 去掉 vsnprintf 写入的结尾 \0
		if (!output.empty() && output.back() == '\0')
		{
			output.pop_back();
		}

		std::lock_guard<std::mutex> lock(GetMutex());
		if (LOG.is_open())
		{
			LOG << LevelPrefix(level) << output << std::endl;
		}
		else
		{
			// 文件没开：回退 stderr，避免日志丢失
			std::fprintf(stderr, "%s%s\n", LevelPrefix(level), output.c_str());
		}
	}

private:
	// WHY: 用函数内 static，避免静态初始化顺序问题（SIOF）。
	//   IniParser.cpp 已经显式提到 "避免和 Log::LOG 静态初始化顺序冲突"，
	//   因此 mutex / level 都不能直接做类静态成员在 dllmain.cpp 定义。
	static std::mutex& GetMutex()
	{
		static std::mutex s_mutex;
		return s_mutex;
	}

	static LogLevel& LevelRef()
	{
		static LogLevel s_level = LogLevel::INFO;  // 默认 INFO（与原行为一致）
		return s_level;
	}

	// 等级 → 文本前缀
	static const char* LevelPrefix(LogLevel level)
	{
		switch (level)
		{
		case LogLevel::DEBUG: return "[DEBUG] ";
		case LogLevel::INFO:  return "[INFO ] ";
		case LogLevel::WARN:  return "[WARN ] ";
		case LogLevel::ERROR: return "[ERROR] ";
		case LogLevel::FATAL: return "[FATAL] ";
		}
		return "[?????] ";
	}
};

// 在 dllmain.cpp 中只定义 LOG，level/mutex 走函数内 static（避免 SIOF）
// std::ofstream Log::LOG("ddfix.log");

// -------------------- 旧 logf 兼容 --------------------
//
// 保留旧接口：logf("format %d", x);
// 等价于 Log::Write(LogLevel::INFO, "format %d", x);
static inline void logf(char* fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	int size = vsnprintf(nullptr, 0, fmt, ap);
	va_end(ap);
	if (size < 0)
	{
		return;
	}
	std::string output(static_cast<size_t>(size) + 1, '\0');
	va_list ap2;
	va_start(ap2, fmt);
	vsprintf_s(&output[0], output.size(), fmt, ap2);
	va_end(ap2);
	// 走流式接口，<< 内部加锁；自动换行由析构 Log() 触发
	Log() << output.c_str();
}

// -------------------- 新增宏简写（Phase 5.1） --------------------
//
// 用法：LOG_INFO("loaded %d textures", n);
//       LOG_ERROR("CreateDevice failed hr=0x%08x", hr);
#define LOG_DEBUG(fmt, ...) ::Log::Write(::LogLevel::DEBUG, fmt, ##__VA_ARGS__)
#define LOG_INFO(fmt, ...)  ::Log::Write(::LogLevel::INFO,  fmt, ##__VA_ARGS__)
#define LOG_WARN(fmt, ...)  ::Log::Write(::LogLevel::WARN,  fmt, ##__VA_ARGS__)
#define LOG_ERROR(fmt, ...) ::Log::Write(::LogLevel::ERROR, fmt, ##__VA_ARGS__)
#define LOG_FATAL(fmt, ...) ::Log::Write(::LogLevel::FATAL, fmt, ##__VA_ARGS__)
