#pragma once

#include <map>
#include <unordered_map>
#include <orion/noncopyable.hpp>
#include <orion/bosen/type.hpp>
#include <orion/bosen/config.hpp>
#include <orion/bosen/julia_module.hpp>
#include <orion/bosen/blob.hpp>
#include <orion/bosen/dist_array_meta.hpp>
#include <orion/bosen/dist_array_access.hpp>
#include <orion/bosen/send_data_buffer.hpp>
#include <orion/bosen/peer_recv_buffer.hpp>

namespace orion {
namespace bosen {
class AbstractDistArrayPartition;
class JuliaEvaluator;

// A DistArray has different access modes depending on
// 1) whether or not a global or local index is built;
// 1) whether it's dense or sparse

// The supported access modes are:
// 1) Seqential: if not index is built
// 2) LocalDenseIndex: if local index is built and dist array partition is dense
// 3) LocalSparseIndex: if local index is built and dist array partition is sparse
// 4) GlobalIndex: if global index is built

// Sequential:
// 1) int64* GetKeys(Partition)
// 2) ValueType* GetValues(Partition)

// LocalDenseIndex:
// 1) ValueType* GetRange()

// LocalSparseIndex:

// GlobalIndex

class DistArray {
 public:
  using TimePartitionMap = std::map<int32_t, AbstractDistArrayPartition*>;
  using SpaceTimePartitionMap = std::map<int32_t, TimePartitionMap>;
  using PartitionMap = std::map<int32_t, AbstractDistArrayPartition*>;
  // using raw pointers to transfer ownership to sender threads which will free
  // the memory
 public:
  const int32_t kId;
  const Config &kConfig;
  const bool kIsServer;
  const type::PrimitiveType kValueType;
  const size_t kValueSize;
  const int32_t kExecutorId;
  const int32_t kServerId;
 private:
  SpaceTimePartitionMap space_time_partitions_;
  PartitionMap partitions_;
  std::vector<int64_t> dims_;
  DistArrayMeta meta_;
  //DistArrayPartitionScheme partition_scheme_;
  DistArrayAccess access_;
  JuliaThreadRequester *julia_requester_;
  std::vector<jl_value_t*> gc_partitions_;
  AbstractDistArrayPartition *buffer_partition_ {nullptr};
 public:
  DistArray(int32_t id,
            const Config& config,
            bool is_server,
            type::PrimitiveType value_type,
            int32_t executor_id,
            int32_t server_id,
            size_t num_dims,
            DistArrayParentType parent_type,
            DistArrayInitType init_type,
            DistArrayMapType map_type,
            DistArrayPartitionScheme partition_scheme,
            JuliaModule map_func_module,
            const std::string &map_func_name,
            type::PrimitiveType random_init_type,
            bool flatten_results,
            bool is_dense,
            const std::string &symbol,
            JuliaThreadRequester *julia_requester);
  ~DistArray();
  int32_t GetId() const { return kId; }

  void LoadPartitionsFromTextFile(std::string file_path);
  void ParseBufferedText(Blob *max_ids,
                         const std::vector<size_t> &line_number_start);
  void GetPartitionTextBufferNumLines(std::vector<int64_t> *partition_ids,
                                      std::vector<size_t> *num_lines);
  void Init();
  void Map(DistArray *child_dist_array);

  void ComputeHashRepartition(size_t num_partitions);
  void ComputeRepartition(const std::string &repartition_func_name);

  void SetDims(const std::vector<int64_t> &dims);
  void SetDims(const int64_t* dims, size_t num_dims);
  std::vector<int64_t> &GetDims();
  DistArrayMeta &GetMeta();
  type::PrimitiveType GetValueType();
  AbstractDistArrayPartition *GetLocalPartition(int32_t partition_id);
  AbstractDistArrayPartition *GetLocalPartition(int32_t space_id,
                                                int32_t time_id);
  std::pair<AbstractDistArrayPartition*, bool> GetAndCreateLocalPartition(int32_t partition_id);
  std::pair<AbstractDistArrayPartition*, bool> GetAndCreateLocalPartition(int32_t space_id,
                                                                          int32_t time_id);
  void GetAndClearLocalPartitions(std::vector<AbstractDistArrayPartition*>
                                  *buff);

  AbstractDistArrayPartition *CreatePartition();
  void RepartitionSerializeAndClear(ExecutorSendBufferMap* send_buff_ptr);
  void RepartitionDeserialize(PeerRecvRepartitionDistArrayDataBuffer *data_buff_ptr);

  void CheckAndBuildIndex();

  void GetMaxPartitionIds(std::vector<int32_t>* ids);

  DistArrayAccess *GetAccessPtr() { return &access_; }
  void SetAccessPartition(int32_t partition_id);
  void SetAccessPartition(AbstractDistArrayPartition *partition);
  void SetBufferAccessPartition();
  void ResetAccessPartition();
  AbstractDistArrayPartition* GetAccessPartition();
  void DeletePartition(int32_t partition_id);
  void DeletePartition(AbstractDistArrayPartition *partition);

  void CreateDistArrayBuffer(const std::string &serialized_value_type);
  void GcPartitions();
 private:
  DISALLOW_COPY(DistArray);
  void RepartitionSerializeAndClearSpaceTime(ExecutorSendBufferMap* send_buff_ptr);
  void RepartitionSerializeAndClear1D(ExecutorSendBufferMap* send_buff_ptr);

  void RepartitionDeserializeInternal(const uint8_t *mem, size_t mem_size);
  void RepartitionDeserializeSpaceTime(const uint8_t *mem, size_t mem_size);
  void RepartitionDeserialize1D(const uint8_t *mem, size_t mem_size);
  void GetMaxPartitionIdsSpaceTime(std::vector<int32_t>* ids);
  void GetMaxPartitionIds1D(std::vector<int32_t>* ids);
  void BuildPartitionIndices();

  void ComputeMaxPartitionIds();
  void ComputeMaxPartitionIdsSpaceTime();
  void ComputeMaxPartitionIds1D();
};

}
}
