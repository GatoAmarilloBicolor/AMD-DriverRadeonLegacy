/*
 * ai_scheduler.cpp - AI Workload Scheduler Implementation
 */
#include "ai_scheduler.h"
#include "memory_manager.h"
#include "ring_manager.h"
#include <algorithm>
#include <chrono>
#include <cmath>

AIScheduler::AIScheduler(MemoryManager *memory, RingManager *rings)
    : fMemory(memory), fRings(rings), fRunning(true)
{
    fSchedulerThread = std::thread(&AIScheduler::scheduling_loop, this);
}

AIScheduler::~AIScheduler() {
    fRunning = false;
    fQueueCV.notify_all();
    if (fSchedulerThread.joinable()) fSchedulerThread.join();
}

int32_t AIScheduler::submit_workload(const AIWorkload &workload, uint64_t *out_id) {
    if (!out_id) return RG_EINVAL;
    
    QueueEntry entry;
    entry.workload = workload;
    entry.workload.id = fNextWorkloadId++;
    entry.enqueue_time = std::chrono::steady_clock::now();
    entry.ready = true;
    
    {
        std::lock_guard<std::mutex> guard(fQueueMutex);
        fQueues[static_cast<size_t>(workload.priority)].push_back(entry);
    }
    fQueueCV.notify_one();
    
    *out_id = workload.id;
    return RG_OK;
}

int32_t AIScheduler::cancel_workload(uint64_t workload_id) {
    std::lock_guard<std::mutex> guard(fQueueMutex);
    for (auto &queue : fQueues) {
        auto it = std::find_if(queue.begin(), queue.end(),
            [workload_id](const QueueEntry &e) { return e.workload.id == workload_id; });
        if (it != queue.end()) {
            queue.erase(it);
            return RG_OK;
        }
    }
    
    // Check running workloads
    std::lock_guard<std::mutex> run_guard(fRunningMutex);
    auto it = fRunning.find(workload_id);
    if (it != fRunning.end()) {
        it->second.workload.completion_cb(CUDA_ERROR_DEINITIALIZED);
        fRunning.erase(it);
        return RG_OK;
    }
    return RG_EINVAL;
}

int32_t AIScheduler::preempt_workload(uint64_t workload_id, uint64_t *checkpoint_id) {
    std::lock_guard<std::mutex> guard(fRunningMutex);
    auto it = fRunning.find(workload_id);
    if (it == fRunning.end()) return RG_EINVAL;
    
    if (!it->second.workload.preemptible) return RG_EPERM;
    
    int32_t st = checkpoint_workload(it->second, checkpoint_id);
    if (st != RG_OK) return st;
    
    it->second.workload.completion_cb(CUDA_ERROR_DEINITIALIZED);
    fRunning.erase(it);
    
    // Re-queue with higher priority if needed
    return RG_OK;
}

int32_t AIScheduler::resume_workload(uint64_t checkpoint_id, uint64_t *new_workload_id) {
    std::lock_guard<std::mutex> guard(fCheckpointMutex);
    auto it = fCheckpoints.find(checkpoint_id);
    if (it == fCheckpoints.end()) return RG_EINVAL;
    
    AIWorkload work = it->second;
    work.id = fNextWorkloadId++;
    work.submit_time_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
    
    return submit_workload(work, new_workload_id);
}

int32_t AIScheduler::get_workload_status(uint64_t workload_id, uint32_t *status, uint64_t *progress) {
    std::lock_guard<std::mutex> guard(fRunningMutex);
    auto it = fRunning.find(workload_id);
    if (it != fRunning.end()) {
        if (status) *status = 1; // Running
        if (progress) {
            auto now = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(
                now - std::chrono::steady_clock::time_point(
                    std::chrono::nanoseconds(it->second.start_time_ns))).count();
            *progress = (it->second.workload.estimated_duration_ns > 0) ? 
                (elapsed * 100 / it->second.workload.estimated_duration_ns) : 0;
        }
        return RG_OK;
    }
    return RG_EINVAL;
}

void AIScheduler::get_stats(SchedulingStats *stats) const {
    std::lock_guard<std::mutex> guard(fStatsMutex);
    *stats = fStats;
}

void AIScheduler::set_policy(SchedulingPolicy policy) {
    fPolicy = policy;
}

void AIScheduler::set_priority_boost(AIWorkloadType type, int boost) {
    // Implementation would adjust priority based on workload type
}

void AIScheduler::set_gang_scheduling(bool enable) {
    fGangScheduling = enable;
}

int32_t AIScheduler::reserve_kv_cache(uint32_t team_id, uint64_t size, uint32_t *handle) {
    std::lock_guard<std::mutex> guard(fKVMutex);
    if (!fMemory) return RG_EINVAL;
    
    // Reserve VRAM for KV cache
    rg_bo_create req{};
    req.size = size;
    req.alignment = 4096;
    req.domains = RG_DOMAIN_VRAM;
    req.flags = 0;
    
    rg_bo_reply rep{};
    int32_t st = fMemory->create_bo(team_id, req, &rep);
    if (st != RG_OK) return st;
    
    uint32_t h = fNextKVHandle++;
    KVCacheReservation res;
    res.handle = h;
    res.team_id = team_id;
    res.size = size;
    res.offset = rep.gpu_addr;
    res.active = true;
    fKVCaches[h] = res;
    
    if (handle) *handle = h;
    return RG_OK;
}

int32_t AIScheduler::release_kv_cache(uint32_t handle) {
    std::lock_guard<std::mutex> guard(fKVMutex);
    auto it = fKVCaches.find(handle);
    if (it == fKVCaches.end()) return RG_EINVAL;
    
    if (fMemory) {
        fMemory->close_bo(it->second.team_id, it->second.handle); // Need handle from reservation
    }
    fKVCaches.erase(it);
    return RG_OK;
}

int32_t AIScheduler::extend_kv_cache(uint32_t handle, uint64_t additional_size) {
    // Extend existing KV cache reservation
    return RG_OK;
}

int32_t AIScheduler::schedule_flash_attention(uint32_t team_id, uint64_t seq_len, 
                                               uint32_t num_heads, uint32_t head_dim,
                                               uint32_t *handle) {
    std::lock_guard<std::mutex> guard(fFlashMutex);
    uint32_t h = fNextFlashHandle++;
    FlashAttentionSlot slot;
    slot.handle = h;
    slot.team_id = team_id;
    slot.seq_len = seq_len;
    slot.num_heads = num_heads;
    slot.head_dim = head_dim;
    slot.active = true;
    fFlashAttentionSlots[h] = slot;
    
    if (handle) *handle = h;
    return RG_OK;
}

int32_t AIScheduler::schedule_quantized_kernel(uint32_t team_id, uint32_t precision, 
                                                uint64_t ops_count, uint32_t *handle) {
    return RG_OK;
}

int32_t AIScheduler::suggest_batch_size(uint32_t team_id, uint64_t model_vram, 
                                         uint32_t *suggested_batch) {
    if (!fMemory || !suggested_batch) return RG_EINVAL;
    
    uint64_t vram_free = fMemory->vram_used(); // Would need free
    uint64_t available = 1024 * 1024 * 1024; // Placeholder
    
    if (model_vram > 0 && available > model_vram) {
        *suggested_batch = std::max<uint32_t>(1, (available - model_vram) / (1024 * 1024));
    } else {
        *suggested_batch = 1;
    }
    return RG_OK;
}

int32_t AIScheduler::schedule_pipeline_stage(uint32_t team_id, uint32_t stage, 
                                              uint32_t num_stages, uint64_t *handle) {
    return RG_OK;
}

void AIScheduler::set_gpu_affinity(uint64_t workload_id, uint32_t gpu_mask) {
    std::lock_guard<std::mutex> guard(fRunningMutex);
    auto it = fRunning.find(workload_id);
    if (it != fRunning.end()) {
        it->second.assigned_gpu_mask = gpu_mask;
    }
}

void AIScheduler::set_power_limit(uint32_t watts) {
    // Would configure GPU power limit via PMFW/SMU
}

void AIScheduler::set_thermal_throttle_temp(uint32_t celsius) {
    fThermalThrottleTemp = celsius;
}

int32_t AIScheduler::enable_dynamic_batching(uint32_t team_id, uint32_t max_batch, 
                                              uint64_t timeout_us) {
    return RG_OK;
}

int32_t AIScheduler::submit_batched_inference(uint32_t team_id, const void **inputs, 
                                               uint32_t batch_size, uint64_t *workload_id) {
    return RG_OK;
}

void AIScheduler::scheduling_loop() {
    while (fRunning) {
        std::unique_lock<std::mutex> lock(fQueueMutex);
        fQueueCV.wait_for(lock, std::chrono::milliseconds(1), [this] {
            for (const auto &q : fQueues) if (!q.empty()) return true;
            return !fRunning;
        });
        
        if (!fRunning) break;
        
        dispatch_ready();
        handle_preemption();
        update_stats();
    }
}

void AIScheduler::dispatch_ready() {
    std::lock_guard<std::mutex> guard(fQueueMutex);
    
    for (int p = 3; p >= 0; --p) {
        auto &queue = fQueues[p];
        while (!queue.empty()) {
            auto entry = queue.front();
            if (!entry.ready) break;
            
            if (can_schedule(entry.workload)) {
                queue.pop_front();
                
                ScheduledWorkload scheduled;
                scheduled.workload = entry.workload;
                scheduled.scheduled_time_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
                    std::chrono::steady_clock::now().time_since_epoch()).count();
                scheduled.start_time_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
                    std::chrono::steady_clock::now().time_since_epoch()).count();
                scheduled.assigned_gpu_mask = 1; // Single GPU for now
                scheduled.preempted = false;
                scheduled.checkpoint_id = 0;
                
                assign_gpus(entry.workload, &scheduled.assigned_gpu_mask);
                
                {
                    std::lock_guard<std::mutex> run_guard(fRunningMutex);
                    fRunning[entry.workload.id] = scheduled;
                }
                
                // Update stats
                std::lock_guard<std::mutex> stat_guard(fStatsMutex);
                fStats.total_scheduled++;
                fStats.avg_queue_time_per_priority[p] = 
                    (fStats.avg_queue_time_per_priority[p] * fStats.total_scheduled + 
                     std::chrono::duration_cast<std::chrono::nanoseconds>(
                        std::chrono::steady_clock::now() - entry.enqueue_time).count()) 
                    / (fStats.total_scheduled + 1);
            } else {
                break; // Cannot schedule, try next priority
            }
        }
    }
}

void AIScheduler::handle_preemption() {
    if (fPolicy != SchedulingPolicy::DEADLINE_AWARE) return;
    
    std::lock_guard<std::mutex> guard(fRunningMutex);
    auto now = std::chrono::steady_clock::now();
    
    for (auto it = fRunning.begin(); it != fRunning.end();) {
        if (it->second.workload.deadline_ns > 0) {
            auto deadline = std::chrono::nanoseconds(it->second.workload.deadline_ns);
            if (now >= deadline && it->second.workload.preemptible) {
                uint64_t checkpoint_id;
                checkpoint_workload(it->second, &checkpoint_id);
                it = fRunning.erase(it);
            } else {
                ++it;
            }
        } else {
            ++it;
        }
    }
}

void AIScheduler::update_stats() {
    std::lock_guard<std::mutex> guard(fStatsMutex);
    fStats.gpu_utilization = 0.5; // Placeholder
    fStats.vram_utilization = fMemory ? 
        double(fMemory->vram_used()) / fMemory->vram_used() : 0.0;
}

bool AIScheduler::can_schedule(const AIWorkload &work) const {
    if (!fMemory) return true;
    
    // Check VRAM availability
    uint64_t vram_needed = work.vram_required;
    uint64_t vram_free = fMemory->vram_used(); // Would need free space
    
    if (work.vram_required > 0) {
        // Simplified check
        return true;
    }
    return true;
}

void AIScheduler::assign_gpus(const AIWorkload &work, uint32_t *gpu_mask) {
    *gpu_mask = 1; // Single GPU for now
}

int32_t AIScheduler::checkpoint_workload(const ScheduledWorkload &work, uint64_t *checkpoint_id) {
    std::lock_guard<std::mutex> guard(fCheckpointMutex);
    uint64_t id = fNextCheckpointId++;
    fCheckpoints[id] = work.workload;
    *checkpoint_id = id;
    return RG_OK;
}

int32_t AIScheduler::restore_checkpoint(uint64_t checkpoint_id, AIWorkload *out_work) {
    std::lock_guard<std::mutex> guard(fCheckpointMutex);
    auto it = fCheckpoints.find(checkpoint_id);
    if (it == fCheckpoints.end()) return RG_EINVAL;
    *out_work = it->second;
    return RG_OK;
}