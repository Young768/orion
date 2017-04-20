#pragma once

#include <stdint.h>
#include <vector>
#include <stx/btree_map>
#include <stdio.h>
#include <algorithm>
#include <glog/logging.h>
#include <orion/bosen/julia_evaluator.hpp>
#include <orion/bosen/blob.hpp>
#include <orion/bosen/config.hpp>
#include <orion/bosen/abstract_dist_array_partition.hpp>
#include <orion/bosen/key.hpp>

#ifdef ORION_USE_HDFS
#include <hdfs.h>
#endif

namespace orion {
namespace bosen {

template<typename ValueType>
class DistArrayPartition : public AbstractDistArrayPartition {
 private:
  std::vector<int64_t> keys_;
  std::vector<ValueType> values_;
  stx::btree_map<int64_t, ValueType> index_;
  bool index_exists_ {false};
  const Config& kConfig;
  const type::PrimitiveType kValueType;

  // temporary to facilitate LoadTextFile
  std::vector<int64_t> key_buff_;
 public:
  DistArrayPartition(const Config &config, type::PrimitiveType value_type);
  ~DistArrayPartition();

  bool LoadTextFile(JuliaEvaluator *julia_eval,
                    const std::string &file_path, int32_t partition_id,
                    bool map,
                    bool flatten_results,
                    size_t num_dims,
                    JuliaModule mapper_func_module,
                    const std::string &mapper_func_name,
                    Blob *max_key);

  void SetDims(const std::vector<int64_t> &dims);
  void Insert(int64_t key, const Blob &buff) { }
  void Get(int64_t key, Blob *buff) { }
  void GetRange(int64_t start, int64_t end, Blob *buff) { }
};

/*----- Specialized for String (const char*) ------*/
template<>
class DistArrayPartition<const char*> : public AbstractDistArrayPartition {
 private:
  std::vector<int64_t> keys_;
  std::vector<char> values_;
  std::vector<size_t> str_offsets_;
  stx::btree_map<int64_t, const char*> index_;
  bool index_exists_ {false};
  const Config& kConfig;
  const type::PrimitiveType kValueType;
 public:
  DistArrayPartition(const Config &config, type::PrimitiveType value_type);
  ~DistArrayPartition();

  bool LoadTextFile(JuliaEvaluator *julia_eval,
                    const std::string &file_path, int32_t partition_id,
                    bool map,
                    bool flatten_results,
                    size_t num_dims,
                    JuliaModule mapper_func_module,
                    const std::string &mapper_func_name,
                    Blob *max_key);
  void SetDims(const std::vector<int64_t> &dims);

  void Insert(int64_t key, const Blob &buff) { }
  void Get(int64_t key, Blob *buff) { }
  void GetRange(int64_t start, int64_t end, Blob *buff) { }
};

/*---- template general implementation -----*/
template<typename ValueType>
DistArrayPartition<ValueType>::DistArrayPartition(
    const Config &config,
    type::PrimitiveType value_type):
    kConfig(config),
    kValueType(value_type) { }

template<typename ValueType>
DistArrayPartition<ValueType>::~DistArrayPartition() { }

template<typename ValueType>
bool
DistArrayPartition<ValueType>::LoadTextFile(
    JuliaEvaluator *julia_eval,
    const std::string &path, int32_t partition_id,
    bool map,
    bool flatten_results,
    size_t num_dims,
    JuliaModule mapper_func_module,
    const std::string &mapper_func_name,
    Blob *max_key) {
  size_t offset = path.find_first_of(':');
  std::string prefix = path.substr(0, offset);
  std::string file_path = path.substr(offset + 3, path.length() - offset - 3);
  std::vector<char> char_buff;
  size_t begin = 0, end = 0;
  bool read = false;
  if (prefix == "hdfs") {
    read = LoadFromHDFS(kConfig.kHdfsNameNode, file_path, partition_id,
                        kConfig.kNumExecutors, kConfig.kMinPartitionSizeKB * 1024,
                        &char_buff, &begin, &end);
  } else if (prefix == "file") {
    read = LoadFromPosixFS(file_path, partition_id,
                           kConfig.kNumExecutors, kConfig.kMinPartitionSizeKB * 1024,
                           &char_buff, &begin, &end);
  } else {
    LOG(FATAL) << "Cannot parse the path specification " << path;
  }

  if (map) {
    Blob value(type::SizeOf(kValueType));
    auto* parser_func = julia_eval->GetFunction(GetJlModule(mapper_func_module),
                                                mapper_func_name.c_str());
    if (num_dims > 0) {
      key_buff_.clear();
      std::vector<int64_t> key(num_dims);
      char *line = strtok(char_buff.data() + begin, "\n");
      while (line != nullptr) {
        julia_eval->ParseString(line, parser_func, kValueType,
                                &key, &value);
        line = strtok(nullptr, "\n");
        for (auto key_ith : key) {
          key_buff_.push_back(key_ith);
        }
        values_.push_back(*((ValueType*) value.data()));
      }
      LOG(INFO) << "data loading and parsing done!";

      if (max_key != nullptr) {
        std::vector<int64_t> max_key_vec(num_dims, 0);
        for (int i = 0; i < num_dims; i++) {
          for (int j = i; j < key_buff_.size(); j += num_dims) {
            max_key_vec[i] = std::max(max_key_vec[i], key_buff_[j]);
          }
        }
        max_key->resize(sizeof(int64_t)*num_dims);
        memcpy(max_key->data(), max_key_vec.data(), sizeof(int64_t)*num_dims);
      }
    } else {
      char *line = strtok(char_buff.data() + begin, "\n");
      while (line != nullptr) {
        julia_eval->ParseStringValueOnly(line, parser_func, kValueType,
                                         &value);
        line = strtok(nullptr, "\n");
        values_.push_back(*((ValueType*) value.data()));
      }
    }
  } else {
    LOG(FATAL) << "This is not the correct data type";
  }

  return read;
}

template<typename ValueType>
void
DistArrayPartition<ValueType>::SetDims(const std::vector<int64_t> &dims) {
  size_t num_dims = dims.size();
  CHECK_EQ(key_buff_.size() / num_dims, values_.size());
  keys_.clear();
  for (int i = 0; i < key_buff_.size(); i += num_dims) {
    int64_t key = key::array_to_int64(dims, key_buff_.data() + i);
    keys_.push_back(key);
  }
  return;
}

/*---- template const char* implementation -----*/
DistArrayPartition<const char*>::DistArrayPartition(
    const Config &config,
    type::PrimitiveType value_type):
    kConfig(config),
    kValueType(value_type) { }

DistArrayPartition<const char*>::~DistArrayPartition() { }

bool
DistArrayPartition<const char*>::LoadTextFile(
    JuliaEvaluator *julia_eval,
    const std::string &path, int32_t partition_id,
    bool map,
    bool flatten_results,
    size_t num_dims,
    JuliaModule mapper_func_module,
    const std::string &mapper_func_name,
    Blob *max_key) {
  LOG(INFO) << __func__;
  size_t offset = path.find_first_of(':');
  std::string prefix = path.substr(0, offset);
  std::string file_path = path.substr(offset + 3, path.length() - offset - 3);
  std::vector<char> char_buff;
  size_t begin = 0, end = 0;
  bool read = false;
  if (prefix == "hdfs") {
    read = LoadFromHDFS(kConfig.kHdfsNameNode, file_path, partition_id,
                        kConfig.kNumExecutors, kConfig.kMinPartitionSizeKB * 1024,
                        &char_buff, &begin, &end);
  } else if (prefix == "file") {
    read = LoadFromPosixFS(file_path, partition_id,
                           kConfig.kNumExecutors, kConfig.kMinPartitionSizeKB * 1024,
                           &char_buff, &begin, &end);
  } else {
    LOG(FATAL) << "Cannot parse the path specification " << path;
  }
  return read;
}

void
DistArrayPartition<const char*>::SetDims(const std::vector<int64_t> &dims) {
}

}
}
