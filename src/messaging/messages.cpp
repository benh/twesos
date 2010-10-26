#include "messages.hpp"

using std::map;
using std::string;

using process::tuples::serializer;
using process::tuples::deserializer;


namespace mesos { namespace internal {


void operator & (serializer& s, const master::state::MasterState *state)
{
  s & (intptr_t &) state;
}


void operator & (deserializer& d, master::state::MasterState *&state)
{
  d & (intptr_t &) state;
}


void operator & (serializer& s, const slave::state::SlaveState *state)
{
  s & (intptr_t &) state;
}


void operator & (deserializer& d, slave::state::SlaveState *&state)
{
  d & (intptr_t &) state;
}


void operator & (serializer& s, const bytes& b)
{
  s & b.s;
}


void operator & (deserializer& d, bytes& b)
{
  d & b.s;
}


void operator & (serializer& s, const FrameworkID& frameworkId)
{
  s & frameworkId.s;
}


void operator & (deserializer& d, FrameworkID& frameworkId)
{
  d & frameworkId.s;
}


void operator & (serializer& s, const SlaveID& slaveId)
{
  s & slaveId.s;
}


void operator & (deserializer& d, SlaveID& slaveId)
{
  d & slaveId.s;
}


void operator & (serializer& s, const OfferID& offerId)
{
  s & offerId.s;
}


void operator & (deserializer& d, OfferID& offerId)
{
  d & offerId.s;
}


void operator & (serializer& s, const TaskState& state)
{
  s & (const int32_t&) state;
}


void operator & (deserializer& s, TaskState& state)
{
  s & (int32_t&) state;
}


void operator & (serializer& s, const SlaveOffer& offer)
{
  s & offer.slaveId;
  s & offer.host;
  s & offer.params;
}


void operator & (deserializer& s, SlaveOffer& offer)
{
  s & offer.slaveId;
  s & offer.host;
  s & offer.params;
}


void operator & (serializer& s, const TaskDescription& task)
{  
  s & task.taskId;
  s & task.slaveId;
  s & task.name;
  s & task.params;
  s & task.data;
}


void operator & (deserializer& s, TaskDescription& task)
{  
  s & task.taskId;
  s & task.slaveId;
  s & task.name;
  s & task.params;
  s & task.data;
}


void operator & (serializer& s, const FrameworkMessage& message)
{
  s & message.slaveId;
  s & message.taskId;
  s & message.data;
}


void operator & (deserializer& s, FrameworkMessage& message)
{
  s & message.slaveId;
  s & message.taskId;
  s & message.data;
}


void operator & (serializer& s, const ExecutorInfo& info)
{
  s & info.uri;
  s & info.params;
  s & info.data;
}


void operator & (deserializer& s, ExecutorInfo& info)
{
  s & info.uri;
  s & info.params;
  s & info.data;
}


void operator & (serializer& s, const Params& params)
{
  const map<string, string>& map = params.getMap();
  s & (int32_t) map.size();
  foreachpair (const string& key, const string& value, map) {
    s & key;
    s & value;
  }
}


void operator & (deserializer& s, Params& params)
{
  map<string, string>& map = params.getMap();
  map.clear();
  int32_t size;
  string key;
  string value;
  s & size;
  for (int32_t i = 0; i < size; i++) {
    s & key;
    s & value;
    map[key] = value;
  }
}


void operator & (serializer& s, const Resources& resources)
{
  s & resources.cpus;
  s & resources.mem;
}


void operator & (deserializer& s, Resources& resources)
{
  s & resources.cpus;
  s & resources.mem;
}

void operator & (serializer& s, const Task& taskInfo)
{
  s & taskInfo.id;
  s & taskInfo.frameworkId;
  s & taskInfo.resources;
  s & taskInfo.state;
  s & taskInfo.name;
  s & taskInfo.message;
  s & taskInfo.slaveId;
}

void operator & (deserializer& s, Task& taskInfo)
{
  s & taskInfo.id;
  s & taskInfo.frameworkId;
  s & taskInfo.resources;
  s & taskInfo.state;
  s & taskInfo.name;
  s & taskInfo.message;
  s & taskInfo.slaveId;
}

}} /* namespace mesos { namespace internal { */
