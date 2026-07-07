#include "WitchEngine/Core/Logger.h"
#include "WitchEngine/Core/Log/ILogSink.h"
#include "WitchEngine/Core/Log/LogView.h"
#include "WitchEngine/Core/Services.h"
#include "Core/Log/LogRecord.h"
#include "Core/Log/TextBuffer.h"
#include <cstdio>
#include <functional>
#include <mutex>
#include <thread>
#include <vector>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

namespace witch::log {

namespace {

LogView MakeView(const LogRecord& record, const TextBuffer& buffer) {
    return LogView{
        .sequence = record.sequence,
        .timestamp = record.timestamp,
        .frameNumber = record.frameNumber,
        .threadId = record.threadId,
        .level = record.level,
        .categoryHash = record.categoryHash,
        .category = buffer.View(record.categoryOffset, record.categoryLength),
        .message = buffer.View(record.messageOffset, record.messageLength),
        .file = record.file,
        .function = record.function,
        .line = record.line,
    };
}

} // namespace

// ── Logger::Impl ─────────────────────────────────────────────────────────────
// LogRecord / TextBuffer / mutex はすべてここに閉じ込める（公開ヘッダに漏らさない）。
// 将来の LockFree 化は Log() の TextBuffer::Append と records_ push が第一候補で、
// この cpp の中だけで完結する。

struct Logger::Impl {
    explicit Impl(const Config& c)
        : config(c),
          buffers{TextBuffer(c.textBufferCapacity), TextBuffer(c.textBufferCapacity)} {
        records.reserve(c.recordCapacity);
    }

    /// Deferred Sink へ未フラッシュレコードを流し、バッファをスワップする。
    /// 呼び出し側が mutex を保持していること。
    void FlushLocked() {
        for (const auto& record : records) {
            const LogView view = MakeView(record, buffers[active]);
            for (auto& sink : sinks) {
                if (sink->Mode() == SinkMode::Deferred) {
                    sink->Write(view);
                }
            }
        }
        records.clear();
        for (auto& sink : sinks) {
            sink->Flush(); // Immediate Sink は既定の no-op
        }
        // ドレイン完了後にのみリセット＆スワップするため、Sink が読み取り中の
        // バッファが書き潰されることは構造上起きない。
        buffers[active].Reset();
        active ^= 1;
    }

    Config config;
    std::mutex mutex;
    TextBuffer buffers[2];
    int active = 0;
    std::vector<LogRecord> records; ///< 未フラッシュ（Deferred 向け）レコード
    std::vector<std::unique_ptr<ILogSink>> sinks;
    LogLevel globalLevel = LogLevel::Trace;
    uint64_t sequence = 0;
    uint64_t frameNumber = 0;
};

// ── Logger ───────────────────────────────────────────────────────────────────

Logger::Logger(const Config& config) : impl_(std::make_unique<Impl>(config)) {}

Logger::~Logger() {
    std::lock_guard lock(impl_->mutex);
    impl_->FlushLocked();
}

void Logger::Log(LogLevel level, Category category, std::string_view message,
                 const std::source_location& loc) {
    std::lock_guard lock(impl_->mutex);

    if (level < impl_->globalLevel) {
        return;
    }

    // 容量が逼迫していたら先に Flush して空きを作る（切り詰めより先にこちら）。
    {
        auto& buffer = impl_->buffers[impl_->active];
        const size_t needed = category.name.size() + message.size();
        const size_t remaining = buffer.Capacity() - buffer.Size();
        if (impl_->records.size() >= impl_->config.recordCapacity || needed > remaining) {
            impl_->FlushLocked();
        }
    }

    auto& buffer = impl_->buffers[impl_->active]; // Flush 後を指し直す

    // 空バッファにすら収まらない病的なサイズは切り詰める。カテゴリを先に確保し、
    // 残りをメッセージへ割り当てる（カテゴリが容量以上なら本文保存は諦め、ハッシュのみ残す）。
    const size_t capacity = buffer.Capacity();
    std::string_view cat = category.name;
    if (cat.size() > capacity) {
        cat = {}; // 保存しきれないカテゴリ本文は捨てる（categoryHash は保持される）
    }
    std::string_view msg = message;
    if (msg.size() > capacity - cat.size()) {
        msg = msg.substr(0, capacity - cat.size());
    }

    LogRecord record{};
    record.sequence = ++impl_->sequence;
    record.timestamp = std::chrono::system_clock::now();
    record.frameNumber = impl_->frameNumber;
    record.threadId = std::hash<std::thread::id>{}(std::this_thread::get_id());
    record.level = level;
    record.categoryHash = category.hash;
    record.file = loc.file_name();
    record.function = loc.function_name();
    record.line = loc.line();

    if (!cat.empty()) {
        if (auto offset = buffer.Append(cat)) {
            record.categoryOffset = *offset;
            record.categoryLength = static_cast<uint32_t>(cat.size());
        }
    }
    if (auto offset = buffer.Append(msg)) {
        record.messageOffset = *offset;
        record.messageLength = static_cast<uint32_t>(msg.size());
    }

    impl_->records.push_back(record);

    const LogView view = MakeView(record, buffer);
    for (auto& sink : impl_->sinks) {
        if (sink->Mode() == SinkMode::Immediate) {
            sink->Write(view);
        }
    }
}

void Logger::AddSink(std::unique_ptr<ILogSink> sink) {
    std::lock_guard lock(impl_->mutex);
    impl_->sinks.push_back(std::move(sink));
}

void Logger::SetGlobalLevel(LogLevel level) {
    std::lock_guard lock(impl_->mutex);
    impl_->globalLevel = level;
}

void Logger::SetFrameNumber(uint64_t frame) {
    std::lock_guard lock(impl_->mutex);
    impl_->frameNumber = frame;
}

void Logger::Flush() {
    std::lock_guard lock(impl_->mutex);
    impl_->FlushLocked();
}

// ── ファサード ───────────────────────────────────────────────────────────────

namespace {

/// Logger サービス未登録時（Engine::Init 前・Shutdown 後）の逃げ道。
/// 従来の Logger と同じ出力先（stdout + デバッガ）へ直接書く。
void FallbackOutput(LogLevel level, std::string_view msg) {
    auto line = std::format("[{:<5}] {}\n", ToString(level), msg);
    std::fputs(line.c_str(), stdout);
#ifdef _WIN32
    OutputDebugStringA(line.c_str());
#endif
}

} // namespace

void Log(LogLevel level, Category category, std::string_view msg, std::source_location loc) {
    if (auto* logger = Services::Instance().logger) {
        logger->Log(level, category, msg, loc);
    } else {
        FallbackOutput(level, msg);
    }
}

void Info(std::string_view msg, std::source_location loc) {
    Log(LogLevel::Info, {}, msg, loc);
}
void Warn(std::string_view msg, std::source_location loc) {
    Log(LogLevel::Warn, {}, msg, loc);
}
void Error(std::string_view msg, std::source_location loc) {
    Log(LogLevel::Error, {}, msg, loc);
}

void Info(Category category, std::string_view msg, std::source_location loc) {
    Log(LogLevel::Info, category, msg, loc);
}
void Warn(Category category, std::string_view msg, std::source_location loc) {
    Log(LogLevel::Warn, category, msg, loc);
}
void Error(Category category, std::string_view msg, std::source_location loc) {
    Log(LogLevel::Error, category, msg, loc);
}

} // namespace witch::log
