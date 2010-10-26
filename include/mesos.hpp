#ifndef __MESOS_HPP__
#define __MESOS_HPP__

#include <map>
#include <string>

#include <mesos_types.hpp>


namespace mesos {


struct bytes
{
  bytes(const char *data = "")
    : s(data) {}

  bytes(const std::string &data)
    : s(data) {}

  bytes(const void *data, size_t length)
    : s((const char *) data, length) {}

  operator std::string () const { return s; }

  const char * data() const { return s.data(); }

  size_t size() const { return s.size(); }

  std::string s;
};


struct TaskDescription
{
  TaskDescription() {}

  TaskDescription(TaskID _taskId, SlaveID _slaveId, const std::string& _name,
                  const std::map<std::string, std::string>& _params,
                  const bytes& _data)
    : taskId(_taskId), slaveId(_slaveId), name(_name),
      params(_params), data(_data) {}

  TaskDescription(TaskID _taskId, SlaveID _slaveId, const std::string& _name,
                  const std::map<std::string, std::string>& _params)
    : taskId(_taskId), slaveId(_slaveId), name(_name), params(_params) {}

  TaskDescription(TaskID _taskId, SlaveID _slaveId, const std::string& _name,
                  const bytes& _data)
    : taskId(_taskId), slaveId(_slaveId), name(_name), data(_data) {}

  TaskID taskId;
  SlaveID slaveId;
  std::string name;
  std::map<std::string, std::string> params;
  bytes data;
};


struct TaskStatus
{
  TaskStatus() {}

  TaskStatus(TaskID _taskId, TaskState _state, const bytes& _data)
    : taskId(_taskId), state(_state), data(_data) {}

  TaskID taskId;
  TaskState state;
  bytes data;
};


struct SlaveOffer
{
  SlaveOffer() {}

  SlaveOffer(SlaveID _slaveId,
             const std::string& _host,
             const std::map<std::string, std::string>& _params)
    : slaveId(_slaveId), host(_host), params(_params) {}

  SlaveID slaveId;
  std::string host;
  std::map<std::string, std::string> params;
};


struct FrameworkMessage
{
  FrameworkMessage() {}

  FrameworkMessage(SlaveID _slaveId, TaskID _taskId, const bytes& _data)
    : slaveId(_slaveId), taskId(_taskId), data(_data) {}

  SlaveID slaveId;
  TaskID taskId;
  bytes data;
};


/**
 * Information used to launch an executor for a framework.
 * This contains an URI to the executor, which may be either an absolute path
 * on a shared file system or a hdfs:// URI, as well as some opaque data
 * passed to the executor's init() callback.
 * In addition, for both local and HDFS executor URIs, Mesos supports packing
 * up multiple files in a .tgz. In this case, the .tgz should contain a single
 * directory (with any name) and there should be a script in this directory
 * called "executor" that will launch the executor.
 */
struct ExecutorInfo
{
  ExecutorInfo() {}

  ExecutorInfo(const std::string& _uri, const bytes& _data,
               const std::map<std::string, std::string>& _params)
    : uri(_uri), data(_data), params(_params) {}

  ExecutorInfo(const std::string& _uri,
               const std::map<std::string, std::string>& _params)
    : uri(_uri), params(_params) {}

  ExecutorInfo(const std::string& _uri, const bytes& _data)
    : uri(_uri), data(_data) {}

  ExecutorInfo(const std::string& _uri)
    : uri(_uri) {}

  std::string uri;
  std::map<std::string, std::string> params;
  bytes data;
};


}

#endif /* __MESOS_HPP__ */
