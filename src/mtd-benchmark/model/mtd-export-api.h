/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * MTD-Benchmark: Export API for experiment data and visualization
 */

#ifndef MTD_EXPORT_API_H
#define MTD_EXPORT_API_H

#include "mtd-common.h"
#include "mtd-event-bus.h"
#include "mtd-domain-manager.h"
#include "mtd-shuffle-controller.h"
#include "mtd-attack-generator.h"
#include "ns3/object.h"
#include "ns3/ptr.h"

#include <string>
#include <map>
#include <vector>
#include <fstream>

namespace ns3 {
namespace mtd {

/**
 * \brief Export format options
 */
enum class ExportFormat {
    JSON,
    CSV,
    YAML
};

/**
 * \brief Export API for experiment data
 * 
 * Provides functionality to export experiment configuration,
 * traffic traces, domain states, and attack/shuffle events.
 */
class ExportApi : public Object
{
public:
    static TypeId GetTypeId();
    
    ExportApi();
    virtual ~ExportApi();
    
    /**
     * \brief Set experiment configuration
     * \param config Experiment configuration
     */
    void SetExperimentConfig(const ExperimentConfig& config);
    
    /**
     * \brief Get experiment configuration
     * \return Experiment configuration
     */
    ExperimentConfig GetExperimentConfig() const;
    
    /**
     * \brief Set domain manager reference
     * \param domainManager Pointer to domain manager
     */
    void SetDomainManager(Ptr<DomainManager> domainManager);
    
    /**
     * \brief Set shuffle controller reference
     * \param shuffleController Pointer to shuffle controller
     */
    void SetShuffleController(Ptr<ShuffleController> shuffleController);
    
    /**
     * \brief Set attack generator reference
     * \param attackGenerator Pointer to attack generator
     */
    void SetAttackGenerator(Ptr<AttackGenerator> attackGenerator);
    
    /**
     * \brief Set event bus reference
     * \param eventBus Pointer to event bus
     */
    void SetEventBus(Ptr<EventBus> eventBus);
    
    /**
     * \brief Export complete experiment snapshot
     * \param path Output file path
     * \param format Export format
     * \return True if successful
     */
    bool ExportExperimentSnapshot(const std::string& path, 
                                   ExportFormat format = ExportFormat::JSON);
    
    /**
     * \brief Export traffic trace
     * \param path Output file path
     * \param format Export format
     * \return True if successful
     */
    bool ExportTrafficTrace(const std::string& path,
                            ExportFormat format = ExportFormat::CSV);
    
    /**
     * \brief Export domain state
     * \param path Output file path
     * \param format Export format
     * \return True if successful
     */
    bool ExportDomainState(const std::string& path,
                           ExportFormat format = ExportFormat::JSON);
    
    /**
     * \brief Export shuffle events
     * \param path Output file path
     * \param format Export format
     * \return True if successful
     */
    bool ExportShuffleEvents(const std::string& path,
                             ExportFormat format = ExportFormat::CSV);
    
    /**
     * \brief Export attack events
     * \param path Output file path
     * \param format Export format
     * \return True if successful
     */
    bool ExportAttackEvents(const std::string& path,
                            ExportFormat format = ExportFormat::CSV);
    
    /**
     * \brief Export event bus history
     * \param path Output file path
     * \param format Export format
     * \return True if successful
     */
    bool ExportEventHistory(const std::string& path,
                            ExportFormat format = ExportFormat::JSON);
    
    /**
     * \brief Record traffic sample
     * \param stats Traffic statistics to record
     * \param domainId Domain ID (0 for global)
     * \param proxyId Proxy ID (0 for aggregate)
     */
    void RecordTrafficSample(const TrafficStats& stats, 
                             uint32_t domainId = 0, uint32_t proxyId = 0);
    
    /**
     * \brief Start auto-recording
     * \param intervalSeconds Recording interval
     */
    void StartAutoRecording(double intervalSeconds);
    
    /**
     * \brief Stop auto-recording
     */
    void StopAutoRecording();
    
    /**
     * \brief Get performance summary
     * \return Map of metric name to value
     */
    std::map<std::string, double> GetPerformanceSummary() const;
    
    /**
     * \brief Clear all recorded data
     */
    void ClearRecords();
    
    /**
     * \brief Set output directory
     * \param directory Output directory path
     */
    void SetOutputDirectory(const std::string& directory);
    
    /**
     * \brief Get output directory
     * \return Output directory path
     */
    std::string GetOutputDirectory() const;

private:
    struct TrafficRecord {
        uint64_t timestamp;
        uint32_t domainId;
        uint32_t proxyId;
        TrafficStats stats;
    };
    
    ExperimentConfig m_config;
    Ptr<DomainManager> m_domainManager;
    Ptr<ShuffleController> m_shuffleController;
    Ptr<AttackGenerator> m_attackGenerator;
    Ptr<EventBus> m_eventBus;
    
    std::vector<TrafficRecord> m_trafficRecords;
    std::string m_outputDirectory;
    EventId m_recordingEvent;
    double m_recordingInterval;
    bool m_autoRecording;
    
    void PerformAutoRecord();
    std::string GenerateJsonSnapshot() const;
    std::string GenerateTrafficCsv() const;
    std::string GenerateDomainJson() const;
    std::string GenerateShuffleCsv() const;
    std::string GenerateAttackCsv() const;
    std::string GenerateEventJson() const;
    
    bool WriteToFile(const std::string& path, const std::string& content);
    std::string EscapeJson(const std::string& str) const;
    std::string EscapeCsv(const std::string& str) const;
};

} // namespace mtd
} // namespace ns3

#endif // MTD_EXPORT_API_H
