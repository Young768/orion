#include <string>
#include <memory>
#include <unordered_map>
#include <iostream>

namespace orion {
class GLogConfig {
 private:
  std::unordered_map<std::string, std::string> map_;
  char* progname_;
  std::string buffer_[6];
  char* argv_[7];
 public:
  GLogConfig(char* progname):
      progname_(progname) {
    map_["logtostderr"] = "false";
    map_["minloglevel"] = "0";
    map_["v"] = "-1";
    map_["stderrthreshold"] = "2";
    map_["alsologtostderr"] = "false";
    map_["log_dir"] = "";
  }

  bool
  set(const char *key, const char*value) {
    auto iter = map_.find(key);
    if (iter == map_.end()) return false;
    iter->second = value;
    return true;
  }

  char **
  get_argv() {
    argv_[0] = progname_;
    int i = 0;
    for (auto& kv : map_) {
      buffer_[i].clear();
      buffer_[i].append("--");
      buffer_[i].append(kv.first);
      buffer_[i].append("=");
      buffer_[i].append(kv.second);
      buffer_[i].append(1, '\0');
      argv_[i + 1] = const_cast<char*>(buffer_[i].c_str());
      i++;
    }
    return argv_;
  }

  int
  get_argc() const {
    return sizeof(argv_) / sizeof(char*);
  }
};
}