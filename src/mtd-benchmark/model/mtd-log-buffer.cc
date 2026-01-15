/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * MTD-Benchmark: Buffered Log Writer Implementation
 */

#include "mtd-log-buffer.h"
#include "ns3/log.h"

#include <algorithm>

namespace ns3 {
namespace mtd {

NS_LOG_COMPONENT_DEFINE("MtdLogBuffer");

// ============================================================================
// LogBuffer Implementation
// ============================================================================

LogBuffer::LogBuffer(LogBufferConfig config)
    : m_config(config)
{
    m_buffer.reserve(m_config.bufferCapacity);
}

LogBuffer::~LogBuffer()
{
    Close();
}

LogBuffer::LogBuffer(LogBuffer&& other) noexcept
    : m_config(std::move(other.m_config)),
      m_buffer(std::move(other.m_buffer)),
      m_file(std::move(other.m_file)),
      m_filePath(std::move(other.m_filePath)),
      m_totalBytesWritten(other.m_totalBytesWritten),
      m_flushCount(other.m_flushCount)
{
    other.m_totalBytesWritten = 0;
    other.m_flushCount = 0;
}

LogBuffer& LogBuffer::operator=(LogBuffer&& other) noexcept
{
    if (this != &other)
    {
        Close();
        m_config = std::move(other.m_config);
        m_buffer = std::move(other.m_buffer);
        m_file = std::move(other.m_file);
        m_filePath = std::move(other.m_filePath);
        m_totalBytesWritten = other.m_totalBytesWritten;
        m_flushCount = other.m_flushCount;
        other.m_totalBytesWritten = 0;
        other.m_flushCount = 0;
    }
    return *this;
}

bool
LogBuffer::Open(const std::string& path)
{
    NS_LOG_FUNCTION(this << path);

    if (m_file.is_open())
    {
        Close();
    }

    // Ensure parent directory exists
    std::filesystem::path fsPath(path);
    std::filesystem::path parentDir = fsPath.parent_path();
    if (!parentDir.empty() && !std::filesystem::exists(parentDir))
    {
        std::error_code ec;
        if (!std::filesystem::create_directories(parentDir, ec))
        {
            NS_LOG_ERROR("Failed to create directory: " << parentDir << " - " << ec.message());
            return false;
        }
    }

    auto mode = std::ios::out | std::ios::binary;
    if (m_config.appendMode)
    {
        mode |= std::ios::app;
    }
    else
    {
        mode |= std::ios::trunc;
    }

    m_file.open(path, mode);
    if (!m_file.is_open())
    {
        NS_LOG_ERROR("Failed to open file: " << path);
        return false;
    }

    m_filePath = path;
    m_buffer.clear();
    m_buffer.reserve(m_config.bufferCapacity);

    NS_LOG_DEBUG("Opened log file: " << path);
    return true;
}

void
LogBuffer::Close()
{
    NS_LOG_FUNCTION(this);

    if (m_file.is_open())
    {
        FlushInternal();
        m_file.close();
        NS_LOG_DEBUG("Closed log file: " << m_filePath 
                     << " (total: " << m_totalBytesWritten << " bytes, "
                     << m_flushCount << " flushes)");
    }
    m_filePath.clear();
}

bool
LogBuffer::IsOpen() const
{
    return m_file.is_open();
}

std::string
LogBuffer::GetPath() const
{
    return m_filePath;
}

void
LogBuffer::Write(const std::string& data)
{
    Write(data.data(), data.size());
}

void
LogBuffer::Write(const char* data, size_t size)
{
    if (size == 0) return;

    // Check if we need to flush first
    if (m_buffer.size() + size > m_config.bufferCapacity)
    {
        FlushInternal();
    }

    // If single write exceeds buffer capacity, write directly
    if (size > m_config.bufferCapacity)
    {
        if (m_file.is_open())
        {
            m_file.write(data, size);
            m_totalBytesWritten += size;
            if (m_config.syncOnFlush)
            {
                m_file.flush();
            }
        }
        return;
    }

    // Append to buffer
    m_buffer.insert(m_buffer.end(), data, data + size);
}

void
LogBuffer::Flush()
{
    FlushInternal();
}

void
LogBuffer::FlushInternal()
{
    if (m_buffer.empty() || !m_file.is_open())
    {
        return;
    }

    m_file.write(m_buffer.data(), m_buffer.size());
    m_totalBytesWritten += m_buffer.size();
    m_buffer.clear();
    ++m_flushCount;

    if (m_config.syncOnFlush)
    {
        m_file.flush();
    }
}

size_t
LogBuffer::GetBufferedSize() const
{
    return m_buffer.size();
}

size_t
LogBuffer::GetTotalBytesWritten() const
{
    return m_totalBytesWritten + m_buffer.size();
}

size_t
LogBuffer::GetFlushCount() const
{
    return m_flushCount;
}

void
LogBuffer::SetConfig(const LogBufferConfig& config)
{
    m_config = config;
}

LogBufferConfig
LogBuffer::GetConfig() const
{
    return m_config;
}

void
LogBuffer::EnsureCapacity(size_t additionalBytes)
{
    if (m_buffer.size() + additionalBytes > m_config.bufferCapacity)
    {
        FlushInternal();
    }
}

// ============================================================================
// LogBufferManager Implementation
// ============================================================================

LogBufferManager::LogBufferManager(LogBufferConfig defaultConfig)
    : m_defaultConfig(defaultConfig)
{
    NS_LOG_FUNCTION(this);
}

LogBufferManager::~LogBufferManager()
{
    NS_LOG_FUNCTION(this);
    CloseAll();
}

LogBuffer&
LogBufferManager::GetBuffer(const std::string& path)
{
    auto it = m_buffers.find(path);
    if (it == m_buffers.end())
    {
        auto buffer = std::make_unique<LogBuffer>(m_defaultConfig);
        buffer->Open(path);
        auto [insertIt, success] = m_buffers.emplace(path, std::move(buffer));
        return *insertIt->second;
    }
    return *it->second;
}

void
LogBufferManager::Write(const std::string& path, const std::string& data)
{
    GetBuffer(path).Write(data);
}

void
LogBufferManager::FlushAll()
{
    NS_LOG_FUNCTION(this);
    for (auto& pair : m_buffers)
    {
        pair.second->Flush();
    }
}

void
LogBufferManager::CloseAll()
{
    NS_LOG_FUNCTION(this);
    for (auto& pair : m_buffers)
    {
        pair.second->Close();
    }
    m_buffers.clear();
}

std::map<std::string, size_t>
LogBufferManager::GetStatistics() const
{
    std::map<std::string, size_t> stats;
    for (const auto& pair : m_buffers)
    {
        stats[pair.first] = pair.second->GetTotalBytesWritten();
    }
    return stats;
}

void
LogBufferManager::SetDefaultConfig(const LogBufferConfig& config)
{
    m_defaultConfig = config;
}

} // namespace mtd
} // namespace ns3
