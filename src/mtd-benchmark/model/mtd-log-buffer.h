/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * MTD-Benchmark: Buffered Log Writer
 * 
 * Provides high-performance buffered I/O for logging to prevent
 * frequent disk writes from impacting simulation performance.
 * Critical for high-frequency DDoS attack scenarios.
 */

#ifndef MTD_LOG_BUFFER_H
#define MTD_LOG_BUFFER_H

#include <string>
#include <fstream>
#include <vector>
#include <memory>
#include <mutex>
#include <filesystem>

namespace ns3 {
namespace mtd {

/**
 * \brief Buffer configuration options
 */
struct LogBufferConfig
{
    size_t bufferCapacity{4096};      ///< Buffer size before auto-flush (4KB default)
    size_t maxFileSize{0};            ///< Max file size before rotation (0 = unlimited)
    bool syncOnFlush{false};          ///< Call fsync after flush (durability vs performance)
    bool appendMode{true};            ///< Append to existing files vs overwrite
    
    static LogBufferConfig Default() { return LogBufferConfig{}; }
    static LogBufferConfig HighPerformance() 
    { 
        LogBufferConfig cfg;
        cfg.bufferCapacity = 16384;  // 16KB buffer
        cfg.syncOnFlush = false;
        return cfg;
    }
    static LogBufferConfig HighDurability()
    {
        LogBufferConfig cfg;
        cfg.bufferCapacity = 1024;   // 1KB buffer (more frequent flush)
        cfg.syncOnFlush = true;
        return cfg;
    }
};

/**
 * \brief Buffered file writer for high-performance logging
 * 
 * Accumulates log data in memory and flushes to disk when:
 * - Buffer reaches capacity threshold
 * - Flush() is explicitly called
 * - Writer is destroyed
 * 
 * Thread-safety: Single-writer assumed (NS-3 is single-threaded).
 * The mutex is included for future extension but minimal overhead.
 */
class LogBuffer
{
public:
    explicit LogBuffer(LogBufferConfig config = LogBufferConfig::Default());
    ~LogBuffer();

    // Non-copyable, movable
    LogBuffer(const LogBuffer&) = delete;
    LogBuffer& operator=(const LogBuffer&) = delete;
    LogBuffer(LogBuffer&&) noexcept;
    LogBuffer& operator=(LogBuffer&&) noexcept;

    /**
     * \brief Open a file for buffered writing
     * \param path File path to open
     * \return true if successful
     */
    bool Open(const std::string& path);

    /**
     * \brief Close the file (flushes buffer first)
     */
    void Close();

    /**
     * \brief Check if file is open
     * \return true if file is open and writable
     */
    bool IsOpen() const;

    /**
     * \brief Get the current file path
     * \return File path or empty if not open
     */
    std::string GetPath() const;

    /**
     * \brief Write data to buffer
     * \param data String data to write
     * 
     * Automatically flushes if buffer exceeds capacity.
     */
    void Write(const std::string& data);

    /**
     * \brief Write raw bytes to buffer
     * \param data Pointer to data
     * \param size Number of bytes
     */
    void Write(const char* data, size_t size);

    /**
     * \brief Force flush buffer to disk
     */
    void Flush();

    /**
     * \brief Get current buffer utilization
     * \return Number of bytes currently buffered
     */
    size_t GetBufferedSize() const;

    /**
     * \brief Get total bytes written (including buffered)
     * \return Total bytes written to this file
     */
    size_t GetTotalBytesWritten() const;

    /**
     * \brief Get number of flush operations performed
     * \return Flush count
     */
    size_t GetFlushCount() const;

    /**
     * \brief Update buffer configuration
     * \param config New configuration
     * 
     * Note: Changes take effect on next write, existing buffer retained.
     */
    void SetConfig(const LogBufferConfig& config);

    /**
     * \brief Get current configuration
     * \return Current buffer configuration
     */
    LogBufferConfig GetConfig() const;

private:
    LogBufferConfig m_config;
    std::vector<char> m_buffer;
    std::ofstream m_file;
    std::string m_filePath;
    size_t m_totalBytesWritten{0};
    size_t m_flushCount{0};

    void FlushInternal();
    void EnsureCapacity(size_t additionalBytes);
};

/**
 * \brief Manager for multiple buffered log files
 * 
 * Provides centralized management of log buffers with:
 * - Lazy file creation (only create when first write occurs)
 * - Coordinated flush across all files
 * - Automatic cleanup on destruction
 */
class LogBufferManager
{
public:
    explicit LogBufferManager(LogBufferConfig defaultConfig = LogBufferConfig::Default());
    ~LogBufferManager();

    // Non-copyable
    LogBufferManager(const LogBufferManager&) = delete;
    LogBufferManager& operator=(const LogBufferManager&) = delete;

    /**
     * \brief Get or create a buffer for a file path
     * \param path File path
     * \return Reference to the LogBuffer
     * 
     * Creates the buffer lazily; file is opened on first write.
     */
    LogBuffer& GetBuffer(const std::string& path);

    /**
     * \brief Write to a specific file
     * \param path File path
     * \param data Data to write
     */
    void Write(const std::string& path, const std::string& data);

    /**
     * \brief Flush all buffers
     */
    void FlushAll();

    /**
     * \brief Close all files
     */
    void CloseAll();

    /**
     * \brief Get statistics across all buffers
     * \return Map of path to bytes written
     */
    std::map<std::string, size_t> GetStatistics() const;

    /**
     * \brief Set default configuration for new buffers
     * \param config Default configuration
     */
    void SetDefaultConfig(const LogBufferConfig& config);

private:
    LogBufferConfig m_defaultConfig;
    std::map<std::string, std::unique_ptr<LogBuffer>> m_buffers;
};

} // namespace mtd
} // namespace ns3

#endif // MTD_LOG_BUFFER_H
