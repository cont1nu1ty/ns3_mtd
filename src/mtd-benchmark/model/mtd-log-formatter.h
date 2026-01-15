/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * MTD-Benchmark: Log Formatter Strategy Pattern
 * 
 * Provides pluggable formatting strategies for event logging.
 * Decouples formatting logic from the EventBus routing/I/O concerns.
 */

#ifndef MTD_LOG_FORMATTER_H
#define MTD_LOG_FORMATTER_H

#include "mtd-common.h"

#include <memory>
#include <sstream>
#include <iomanip>
#include <ctime>

namespace ns3 {
namespace mtd {

/**
 * \brief Abstract base class for log formatters (Strategy Pattern)
 * 
 * Implementations define how MtdEvent objects are serialized to strings.
 * This allows easy switching between human-readable, JSON, or custom formats.
 */
class LogFormatter
{
public:
    virtual ~LogFormatter() = default;

    /**
     * \brief Format an event to a string
     * \param event The event to format
     * \return Formatted string representation
     */
    virtual std::string Format(const MtdEvent& event) = 0;

    /**
     * \brief Get the file extension for this format
     * \return File extension (e.g., ".log", ".json", ".jsonl")
     */
    virtual std::string GetFileExtension() const = 0;

    /**
     * \brief Get format name for identification
     * \return Format name string
     */
    virtual std::string GetFormatName() const = 0;
};

/**
 * \brief Text formatter for human-readable log output
 * 
 * Produces logs in the format:
 * [123.456ms] EVENT_TYPE node=5 {key1=val1, key2=val2}
 */
class TextFormatter : public LogFormatter
{
public:
    TextFormatter() = default;
    ~TextFormatter() override = default;

    std::string Format(const MtdEvent& event) override
    {
        std::ostringstream ss;
        ss << std::fixed << std::setprecision(3);
        
        // Timestamp in milliseconds
        double timeMs = static_cast<double>(event.timestamp);
        ss << "[" << timeMs << "ms] ";
        
        // Event type
        ss << EventTypeToString(event.type);
        
        // Source node
        ss << " node=" << event.sourceNodeId;
        
        // Metadata (if any)
        if (!event.metadata.empty())
        {
            ss << " {";
            bool first = true;
            for (const auto& kv : event.metadata)
            {
                if (!first) ss << ", ";
                ss << kv.first << "=" << kv.second;
                first = false;
            }
            ss << "}";
        }
        
        ss << "\n";
        return ss.str();
    }

    std::string GetFileExtension() const override { return ".log"; }
    std::string GetFormatName() const override { return "text"; }
};

/**
 * \brief JSON Lines formatter for machine-readable output
 * 
 * Produces one JSON object per line (JSONL format), ideal for:
 * - Python pandas: pd.read_json(file, lines=True)
 * - Streaming parsers
 * - Log aggregation systems
 */
class JsonFormatter : public LogFormatter
{
public:
    JsonFormatter() = default;
    ~JsonFormatter() override = default;

    std::string Format(const MtdEvent& event) override
    {
        std::ostringstream ss;
        ss << std::fixed << std::setprecision(3);
        
        ss << "{";
        ss << "\"timestamp\":" << event.timestamp << ",";
        ss << "\"type\":\"" << EventTypeToString(event.type) << "\",";
        ss << "\"typeId\":" << static_cast<int>(event.type) << ",";
        ss << "\"sourceNodeId\":" << event.sourceNodeId;
        
        // Metadata as nested object
        if (!event.metadata.empty())
        {
            ss << ",\"metadata\":{";
            bool first = true;
            for (const auto& kv : event.metadata)
            {
                if (!first) ss << ",";
                ss << "\"" << EscapeJson(kv.first) << "\":\"" 
                   << EscapeJson(kv.second) << "\"";
                first = false;
            }
            ss << "}";
        }
        
        ss << "}\n";
        return ss.str();
    }

    std::string GetFileExtension() const override { return ".jsonl"; }
    std::string GetFormatName() const override { return "json"; }

private:
    static std::string EscapeJson(const std::string& str)
    {
        std::string result;
        result.reserve(str.size());
        for (char c : str)
        {
            switch (c)
            {
                case '"': result += "\\\""; break;
                case '\\': result += "\\\\"; break;
                case '\n': result += "\\n"; break;
                case '\r': result += "\\r"; break;
                case '\t': result += "\\t"; break;
                default: result += c; break;
            }
        }
        return result;
    }
};

/**
 * \brief Compact text formatter for timeline view
 * 
 * Produces minimal output for quick timeline scanning:
 * 123.456 ATTACK_DETECTED proxy=3
 */
class CompactFormatter : public LogFormatter
{
public:
    CompactFormatter() = default;
    ~CompactFormatter() override = default;

    std::string Format(const MtdEvent& event) override
    {
        std::ostringstream ss;
        ss << std::fixed << std::setprecision(3);
        
        double timeMs = static_cast<double>(event.timestamp);
        ss << timeMs << " " << EventTypeToString(event.type);
        
        // Include key metadata inline
        if (event.metadata.count("proxyId"))
        {
            ss << " proxy=" << event.metadata.at("proxyId");
        }
        if (event.metadata.count("userId"))
        {
            ss << " user=" << event.metadata.at("userId");
        }
        if (event.metadata.count("domainId"))
        {
            ss << " domain=" << event.metadata.at("domainId");
        }
        
        ss << "\n";
        return ss.str();
    }

    std::string GetFileExtension() const override { return ".timeline"; }
    std::string GetFormatName() const override { return "compact"; }
};

// Type alias for formatter pointers
using LogFormatterPtr = std::unique_ptr<LogFormatter>;

/**
 * \brief Factory function to create formatters by name
 * \param name Formatter name ("text", "json", "compact")
 * \return Unique pointer to formatter instance
 */
inline LogFormatterPtr CreateFormatter(const std::string& name)
{
    if (name == "json" || name == "jsonl")
    {
        return std::make_unique<JsonFormatter>();
    }
    else if (name == "compact" || name == "timeline")
    {
        return std::make_unique<CompactFormatter>();
    }
    // Default to text
    return std::make_unique<TextFormatter>();
}

} // namespace mtd
} // namespace ns3

#endif // MTD_LOG_FORMATTER_H
