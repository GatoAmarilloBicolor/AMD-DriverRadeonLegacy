/*
 * ai_scheduler.h - AI Workload Scheduler for AMD GPUs
 * 
 * Features:
 * - Workload prioritization (inference > training > compilation)
 * - Gang scheduling for multi-GPU/model parallel workloads
 * - Preemption with checkpoint/restore
 * - Dynamic batch sizing based on VRAM pressure
 * - Pipeline parallelism scheduling
 * - KV cache management
 * - Flash attention scheduling
 * - Quantization-aware scheduling
 */
#ifndef RG_AI_SCHEDULER_H
#define RG_AI_SCHEDULER_H

#include <stdint.h>
#include <vector>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <functional>
#include <chrono>
#include <memory>

#include "../common/rg_types.h"
#include "memory_manager.h"

enum class AIWorkloadPriority : uint8_t {
    LOW        = 0,  // Batch training, compilation
    NORMAL     = 1,  // Standard inference
    HIGH       = 2,  // Real-time inference, RL
    CRITICAL   = 3,  // Safety-critical, autonomous systems
};

enum class AIWorkloadType : uint8_t {
    INFERENCE      = 0,
    TRAINING       = 1,
    FINE_TUNING    = 2,
    COMPILATION    = 3,
    QUANTIZATION   = 3,
    DISTILLATION   = 4,
};

enum class SchedulingPolicy : uint8_t {
    FIFO           = 0,
    PRIORITY       = 1,
    FAIR_SHARE     = 2,
    DEADLINE_AWARE = 3,
    GANG           = 4,  // Gang scheduling for distributed
};

struct AIWorkload {
    uint64_t id;
    AIWorkloadType type;
    AIWorkloadPriority priority;
    uint32_t team_id;
    uint64_t submit_time_ns;
    uint64_t deadline_ns;  // 0 = no deadline
    uint64_t estimated_duration_ns;
    uint64_t vram_required;
    uint64_t gtt_required;
    uint32_t num_gpus;  // For gang scheduling
    uint32_t gang_id;   // 0 = no gang
    bool preemptible;
    bool checkpointable;
    void *user_data;
    std::function<void(uint32_t status)> completion_cb;
};

struct ScheduledWorkload {
    AIWorkload workload;
    uint64_t scheduled_time_ns;
    uint64_t start_time_ns;
    uint64_t completion_time_ns;
    uint32_t assigned_gpu_mask;
    bool preempted;
    uint64_t checkpoint_id;
};

struct SchedulingStats {
    uint64_t total_scheduled;
    uint64_t total_completed;
    uint64_t total_preempted;
    uint64_t total_failed;
    uint64_t avg_queue_time_ns;
    uint64_t avg_execution_time_ns;
    uint64_t avg_queue_time_per_priority[4];
    double gpu_utilization;
    double vram_utilization;
};

class AIScheduler {
public:
    AIScheduler(MemoryManager *memory, RingManager *rings);
    ~AIScheduler();

    // Submit workload for scheduling
    int32_t submit_workload(const AIWorkload &workload, uint64_t *out_id);
    
    // Cancel pending workload
    int32_t cancel_workload(uint64_t workload_id);
    
    // Preempt running workload (for higher priority)
    int32_t preempt_workload(uint64_t workload_id, uint64_t *checkpoint_id);
    
    // Resume from checkpoint
    int32_t resume_workload(uint64_t checkpoint_id, uint64_t *new_workload_id);
    
    // Query status
    int32_t get_workload_status(uint64_t workload_id, uint32_t *status, uint64_t *progress);
    
    // Get scheduler statistics
    void get_stats(SchedulingStats *stats) const;
    
    // Configure scheduling policy
    void set_policy(SchedulingPolicy policy);
    void set_priority_boost(AIWorkloadType type, int boost);
    void set_gang_scheduling(bool enable);
    
    // KV cache management
    int32_t reserve_kv_cache(uint32_t team_id, uint64_t size, uint32_t *handle);
    int32_t release_kv_cache(uint32_t handle);
    int32_t extend_kv_cache(uint32_t handle, uint64_t additional_size);
    
    // Flash attention scheduling
    int32_t schedule_flash_attention(uint32_t team_id, uint64_t seq_len, 
                                      uint32_t num_heads, uint32_t head_dim,
                                      uint32_t *handle);
    
    // Quantization-aware scheduling
    int32_t schedule_quantized_kernel(uint32_t team_id, uint32_t precision, 
                                       uint64_t ops_count, uint32_t *handle);
    
    // Dynamic batch sizing
    int32_t suggest_batch_size(uint32_t team_id, uint64_t model_vram, 
                                uint32_t *suggested_batch);
    
    // Pipeline parallelism
    int32_t schedule_pipeline_stage(uint32_t team_id, uint32_t stage, 
                                     uint32_t num_stages, uint64_t *handle);
    
    // GPU affinity
    void set_gpu_affinity(uint64_t workload_id, uint32_t gpu_mask);
    
    // Power/thermal awareness
    void set_power_limit(uint32_t watts);
    void set_thermal_throttle_temp(uint32_t celsius);

    // Dynamic batching for inference
    int32_t enable_dynamic_batching(uint32_t team_id, uint32_t max_batch, 
                                     uint64_t timeout_us);
    int32_t submit_batched_inference(uint32_t team_id, const void **inputs, 
                                      uint32_t batch_size, uint64_t *workload_id);

private:
    struct QueueEntry {
        AIWorkload workload;
        std::chrono::steady_clock::time_point enqueue_time;
        bool ready;
    };
    
    void scheduling_loop();
    void dispatch_ready();
    void handle_preemption();
    void update_stats();
    bool can_schedule(const AIWorkload &work) const;
    void assign_gpus(const AIWorkload &work, uint32_t *gpu_mask);
    int32_t checkpoint_workload(const ScheduledWorkload &work, uint64_t *checkpoint_id);
    int32_t restore_checkpoint(uint64_t checkpoint_id, AIWorkload *out_work);
    
    MemoryManager *fMemory;
    RingManager *fRings;
    SchedulingPolicy fPolicy = SchedulingPolicy::PRIORITY;
    bool fGangScheduling = false;
    std::atomic<uint64_t> fNextWorkloadId{1};
    std::atomic<uint64_t> fNextCheckpointId{1};
    
    // Priority queues (one per priority level)
    std::array<std::deque<QueueEntry>, 4> fQueues;
    mutable std::mutex fQueueMutex;
    std::condition_variable fQueueCV;
    
    // Running workloads
    std::unordered_map<uint64_t, ScheduledWorkload> fRunning;
    mutable std::mutex fRunningMutex;
    
    // Checkpoints
    std::unordered_map<uint64_t, AIWorkload> fCheckpoints;
    mutable std::mutex fCheckpointMutex;
    
    // KV cache reservations
    struct KVCacheReservation {
        uint32_t handle;
        uint32_t team_id;
        uint64_t size;
        uint64_t offset;
        bool active;
    };
    std::unordered_map<uint32_t, KVCacheReservation> fKVCaches;
    uint32_t fNextKVHandle = 1;
    mutable std::mutex fKVMutex;
    
    // Flash attention slots
    struct FlashAttentionSlot {
        uint32_t handle;
        uint32_t team_id;
        uint64_t seq_len;
        uint32_t num_heads;
        uint32_t head_dim;
        bool active;
    };
    std::unordered_map<uint32_t, FlashAttentionSlot> fFlashAttentionSlots;
    uint32_t fNextFlashHandle = 1;
    mutable std::mutex fFlashMutex;
    
    // Stats
    mutable SchedulingStats fStats;
    mutable std::mutex fStatsMutex;
    
    std::thread fSchedulerThread;
    std::atomic<bool> fRunning{false};
    
    // Power/thermal
    uint32_t fPowerLimitWatts = 0;
    uint32_t fThermalThrottleTemp = 90;
};

#endif /* RG_AI_SCHEDULER_H */