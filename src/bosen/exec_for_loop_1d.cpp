#include <orion/bosen/exec_for_loop_1d.hpp>

namespace orion {
namespace bosen {

ExecForLoop1D::ExecForLoop1D(
      int32_t executor_id,
      size_t num_executors,
      size_t num_servers,
      int32_t iteration_space_id,
      const int32_t *space_partitioned_dist_array_ids,
      size_t num_space_partitioned_dist_arrays,
      const int32_t *global_indexed_dist_array_ids,
      size_t num_global_indexed_dist_arrays,
      const int32_t *buffered_dist_array_ids,
      size_t num_buffered_dist_arrays,
      const int32_t *dist_array_buffer_ids,
      const size_t *num_buffers_each_dist_array,
      const char* loop_batch_func_name,
      const char *prefetch_batch_func_name,
      std::unordered_map<int32_t, DistArray> *dist_arrays,
      std::unordered_map<int32_t, DistArray> *dist_array_buffers):
    AbstractExecForLoop(
        executor_id,
        num_executors,
        num_servers,
        iteration_space_id,
        space_partitioned_dist_array_ids,
        num_space_partitioned_dist_arrays,
        nullptr,
        0,
        global_indexed_dist_array_ids,
        num_global_indexed_dist_arrays,
        buffered_dist_array_ids,
        num_buffered_dist_arrays,
        dist_array_buffer_ids,
        num_buffers_each_dist_array,
        loop_batch_func_name,
        prefetch_batch_func_name,
        dist_arrays,
        dist_array_buffers) {
  auto &meta = iteration_space_->GetMeta();
  auto &max_ids = meta.GetMaxPartitionIds();
  CHECK(max_ids.size() == 1) << "max_ids.size() = " << max_ids.size();
  kMaxPartitionId = max_ids[0];
  kNumClocks = (kMaxPartitionId + kNumExecutors - 1) / kNumExecutors;

  clock_ = 0;
  ComputePartitionIdsAndFindPartitionToExecute();
}

ExecForLoop1D::~ExecForLoop1D() { }

AbstractExecForLoop::RunnableStatus
ExecForLoop1D::GetCurrPartitionRunnableStatus() {
  if (clock_ == kNumClocks) return AbstractExecForLoop::RunnableStatus::kCompleted;
  if (curr_partition_ == nullptr) return AbstractExecForLoop::RunnableStatus::kSkip;
  if (!global_indexed_dist_arrays_.empty()) {
    if (!HasSentAllPrefetchRequests()) return AbstractExecForLoop::RunnableStatus::kPrefetchGlobalIndexedDistArrays;
    if (!HasRecvedAllPrefetches()) return AbstractExecForLoop::RunnableStatus::kAwaitGlobalIndexedDistArrays;
  }
  return AbstractExecForLoop::RunnableStatus::kRunnable;
}

void
ExecForLoop1D::FindNextToExecPartition() {
  if (clock_ == kNumClocks) return;
  clock_++;
  if (clock_ == kNumClocks) return;
  ComputePartitionIdsAndFindPartitionToExecute();
}

void
ExecForLoop1D::ComputePartitionIdsAndFindPartitionToExecute() {
  curr_partition_id_ = clock_ * kNumExecutors + kExecutorId;
  curr_partition_ = iteration_space_->GetLocalPartition(curr_partition_id_);
}

void
ExecForLoop1D::PrepareToExecCurrPartition() {
  if (curr_partition_prepared_) return;
  for (auto& dist_array_pair : space_partitioned_dist_arrays_) {
    auto* dist_array = dist_array_pair.second;
    dist_array->SetAccessPartition(curr_partition_id_);
  }

  for (auto& dist_array_pair : global_indexed_dist_arrays_) {
    auto *dist_array = dist_array_pair.second;
    auto dist_array_id = dist_array_pair.first;
    auto *cache_partition = dist_array_cache_.at(dist_array_id).second;
    dist_array->SetAccessPartition(cache_partition);
  }

  for (auto& buffer_pair : dist_array_buffers_) {
    auto* dist_array_buffer = buffer_pair.second;
    dist_array_buffer->SetBufferAccessPartition();
  }
  curr_partition_prepared_ = true;
}

}
}