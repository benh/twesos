#include <iomanip>

#include <glog/logging.h>

#include "common/date_utils.hpp"

#include "allocator.hpp"
#include "allocator_factory.hpp"
#include "master.hpp"
#include "webui.hpp"

using std::endl;
using std::make_pair;
using std::map;
using std::max;
using std::min;
using std::ostringstream;
using std::pair;
using std::set;
using std::setfill;
using std::setw;
using std::string;
using std::vector;

using boost::lexical_cast;
using boost::unordered_map;
using boost::unordered_set;

using namespace mesos;
using namespace mesos::internal;
using namespace mesos::internal::master;


namespace {

// A process that periodically pings the master to check filter expiries, etc
class AllocatorTimer : public MesosProcess
{
private:
  const PID master;

protected:
  void operator () ()
  {
    link(master);
    do {
      switch (receive(1)) {
      case PROCESS_TIMEOUT:
	send(master, pack<M2M_TIMER_TICK>());
	break;
      case PROCESS_EXIT:
	return;
      }
    } while (true);
  }

public:
  AllocatorTimer(const PID &_master) : master(_master) {}
};


// A process that periodically prints frameworks' shares to a file
class SharesPrinter : public MesosProcess
{
protected:
  PID master;

  void operator () ()
  {
    int tick = 0;

    std::ofstream file ("/mnt/shares");
    if (!file.is_open())
      LOG(FATAL) << "Could not open /mnt/shares";

    while (true) {
      pause(1);

      send(master, pack<M2M_GET_STATE>());
      receive();
      CHECK(msgid() == M2M_GET_STATE_REPLY);
      state::MasterState *state = unpack<M2M_GET_STATE_REPLY, 0>(body());

      uint32_t total_cpus = 0;
      uint32_t total_mem = 0;

      foreach (state::Slave *s, state->slaves) {
        total_cpus += s->cpus;
        total_mem += s->mem;
      }
      
      if (state->frameworks.empty()) {
        file << "--------------------------------" << std::endl;
      } else {
        foreach (state::Framework *f, state->frameworks) {
          double cpu_share = f->cpus / (double) total_cpus;
          double mem_share = f->mem / (double) total_mem;
          double max_share = max(cpu_share, mem_share);
          file << tick << "#" << f->id << "#" << f->name << "#" 
               << f->cpus << "#" << f->mem << "#"
               << cpu_share << "#" << mem_share << "#" << max_share << endl;
        }
      }
      delete state;
      tick++;
    }
    file.close();
  }

public:
  SharesPrinter(const PID &_master) : master(_master) {}
  ~SharesPrinter() {}
};

}


Master::Master()
  : nextFrameworkId(0), nextSlaveId(0), nextSlotOfferId(0)
{
  allocatorType = "simple";
}


Master::Master(const Params& conf_)
  : conf(conf_), nextFrameworkId(0), nextSlaveId(0), nextSlotOfferId(0)
{
  allocatorType = conf.get("allocator", "simple");
}
                   

Master::~Master()
{
  LOG(INFO) << "Shutting down master";

  delete allocator;

  foreachpair (_, Framework *framework, frameworks) {
    foreachpair(_, Task *task, framework->tasks)
      delete task;
    delete framework;
  }

  foreachpair (_, Slave *slave, slaves) {
    delete slave;
  }

  foreachpair (_, SlotOffer *offer, slotOffers) {
    delete offer;
  }
}


void Master::registerOptions(Configurator* conf)
{
  conf->addOption<string>("allocator", 'a', "Allocation module name", "simple");
  conf->addOption<bool>("root_submissions",
                        "Can root submit frameworks?",
                        true);
}


state::MasterState * Master::getState()
{
  std::ostringstream oss;
  oss << self();
  state::MasterState *state =
    new state::MasterState(BUILD_DATE, BUILD_USER, oss.str());

  foreachpair (_, Slave *s, slaves) {
    state::Slave *slave = new state::Slave(s->id, s->hostname, s->publicDns,
        s->resources.cpus, s->resources.mem, s->connectTime);
    state->slaves.push_back(slave);
  }

  foreachpair (_, Framework *f, frameworks) {
    state::Framework *framework = new state::Framework(f->id, f->user,
        f->name, f->executorInfo.uri, f->resources.cpus, f->resources.mem,
        f->connectTime);
    state->frameworks.push_back(framework);
    foreachpair (_, Task *t, f->tasks) {
      state::Task *task = new state::Task(t->id, t->name, t->frameworkId,
          t->slaveId, t->state, t->resources.cpus, t->resources.mem);
      framework->tasks.push_back(task);
    }
    foreach (SlotOffer *o, f->slotOffers) {
      state::SlotOffer *offer = new state::SlotOffer(o->id, o->frameworkId);
      foreach (SlaveResources &r, o->resources) {
        state::SlaveResources *resources = new state::SlaveResources(
            r.slave->id, r.resources.cpus, r.resources.mem);
        offer->resources.push_back(resources);
      }
      framework->offers.push_back(offer);
    }
  }
  
  return state;
}


// Return connected frameworks that are not in the process of being removed
vector<Framework *> Master::getActiveFrameworks()
{
  vector <Framework *> result;
  foreachpair(_, Framework *framework, frameworks)
    if (framework->active)
      result.push_back(framework);
  return result;
}


// Return connected slaves that are not in the process of being removed
vector<Slave *> Master::getActiveSlaves()
{
  vector <Slave *> result;
  foreachpair(_, Slave *slave, slaves)
    if (slave->active)
      result.push_back(slave);
  return result;
}


Framework * Master::lookupFramework(FrameworkID fid)
{
  unordered_map<FrameworkID, Framework *>::iterator it =
    frameworks.find(fid);
  if (it != frameworks.end())
    return it->second;
  else
    return NULL;
}


Slave * Master::lookupSlave(SlaveID sid)
{
  unordered_map<SlaveID, Slave *>::iterator it =
    slaves.find(sid);
  if (it != slaves.end())
    return it->second;
  else
    return NULL;
}


SlotOffer * Master::lookupSlotOffer(OfferID oid)
{
  unordered_map<OfferID, SlotOffer *>::iterator it =
    slotOffers.find(oid);
  if (it != slotOffers.end()) 
    return it->second;
  else
    return NULL;
}


void Master::operator () ()
{
  LOG(INFO) << "Master started at mesos://" << self();

  // Don't do anything until we get a master ID.
  while (receive() != GOT_MASTER_ID) {
    LOG(INFO) << "Oops! We're dropping a message since "
              << "we haven't received an identifier yet!";  
  }
  string faultToleranceId;
  tie(faultToleranceId) = unpack<GOT_MASTER_ID>(body());
  masterId = DateUtils::currentDate() + "-" + faultToleranceId;
  LOG(INFO) << "Master ID: " << masterId;

  // Create the allocator (we do this after the constructor because it
  // leaks 'this').
  allocator = createAllocator();
  if (!allocator)
    LOG(FATAL) << "Unrecognized allocator type: " << allocatorType;

  link(spawn(new AllocatorTimer(self())));
  //link(spawn(new SharesPrinter(self())));

  while (true) {
    switch (receive()) {

    case NEW_MASTER_DETECTED: {
      // TODO(benh): We might have been the master, but then got
      // partitioned, and now we are finding out once we reconnect
      // that we are no longer the master, so we should just die.
      LOG(INFO) << "New master detected ... maybe it's us!";
      break;
    }

    case NO_MASTER_DETECTED: {
      LOG(INFO) << "No master detected ... maybe we're next!";
      break;
    }

    case F2M_REGISTER_FRAMEWORK: {
      FrameworkID fid = newFrameworkId();
      Framework *framework = new Framework(from(), fid, elapsed());

      tie(framework->name, framework->user, framework->executorInfo) =
        unpack<F2M_REGISTER_FRAMEWORK>(body());

      LOG(INFO) << "Registering " << framework << " at " << framework->pid;

      if (framework->executorInfo.uri == "") {
        LOG(INFO) << framework << " registering without an executor URI";
        send(framework->pid, pack<M2F_ERROR>(1, "No executor URI given"));
        delete framework;
        break;
      }

      bool rootSubmissions = conf.get<bool>("root_submissions", true);
      if (framework->user == "root" && rootSubmissions == false) {
        LOG(INFO) << framework << " registering as root, but "
                  << "root submissions are disabled on this cluster";
        send(framework->pid, pack<M2F_ERROR>(
              1, "Root is not allowed to submit jobs on this cluster"));
        delete framework;
        break;
      }

      addFramework(framework);
      break;
    }

    case F2M_REREGISTER_FRAMEWORK: {
      Framework *framework = new Framework(from(), "", elapsed());
      int32_t generation;

      tie(framework->id, framework->name, framework->user,
          framework->executorInfo, generation) =
        unpack<F2M_REREGISTER_FRAMEWORK>(body());

      if (framework->executorInfo.uri == "") {
        LOG(INFO) << framework << " re-registering without an executor URI";
        send(framework->pid, pack<M2F_ERROR>(1, "No executor URI given"));
        delete framework;
        break;
      }

      if (framework->id == "") {
        LOG(ERROR) << "Framework re-registering without an id!";
        send(framework->pid, pack<M2F_ERROR>(1, "Missing framework id"));
        delete framework;
        break;
      }

      LOG(INFO) << "Re-registering " << framework << " at " << framework->pid;

      if (frameworks.count(framework->id) > 0) {
        if (generation == 0) {
          LOG(INFO) << framework << " failed over";
          replaceFramework(frameworks[framework->id], framework);
        } else {
          LOG(INFO) << framework << " re-registering with an already "
		    << "used id and not failing over!";
          send(framework->pid, pack<M2F_ERROR>(1, "Framework id in use"));
          delete framework;
          break;
        }
      } else {
        addFramework(framework);
      }

      CHECK(frameworks.find(framework->id) != frameworks.end());

      // Reunite running tasks with this framework. This may be
      // necessary because a framework scheduler has failed over, or
      // the master has failed over and the framework schedulers are
      // reregistering.
      foreachpair (SlaveID slaveId, Slave *slave, slaves) {
        foreachpair (_, Task *task, slave->tasks) {
          if (framework->id == task->frameworkId) {
            framework->addTask(task);
          }
        }
      }

      // Broadcast the new framework pid to all the slaves. We have to
      // broadcast because an executor might be running on a slave but
      // it currently isn't running any tasks. This could be a
      // potential scalability issue ...
      foreachpair (_, Slave *slave, slaves)
	send(slave->pid, pack<M2S_UPDATE_FRAMEWORK_PID>(framework->id,
							framework->pid));
      break;
    }

    case F2M_UNREGISTER_FRAMEWORK: {
      FrameworkID fid;
      tie(fid) = unpack<F2M_UNREGISTER_FRAMEWORK>(body());
      LOG(INFO) << "Asked to unregister framework " << fid;
      Framework *framework = lookupFramework(fid);
      if (framework != NULL && framework->pid == from())
        removeFramework(framework);
      else
        LOG(WARNING) << "Non-authoratative PID attempting framework "
                     << "unregistration ... ignoring";
      break;
    }

    case F2M_SLOT_OFFER_REPLY: {
      FrameworkID fid;
      OfferID oid;
      vector<TaskDescription> tasks;
      Params params;
      tie(fid, oid, tasks, params) = unpack<F2M_SLOT_OFFER_REPLY>(body());
      Framework *framework = lookupFramework(fid);
      if (framework != NULL) {
        SlotOffer *offer = lookupSlotOffer(oid);
        if (offer != NULL) {
          processOfferReply(offer, tasks, params);
        } else {
          // The slot offer is gone, meaning that we rescinded it or that
          // the slave was lost; immediately report any tasks in it as lost
          foreach (const TaskDescription &t, tasks) {
            send(framework->pid,
                 pack<M2F_STATUS_UPDATE>(t.taskId, TASK_LOST, ""));
          }
        }
      }
      break;
    }

    case F2M_REVIVE_OFFERS: {
      FrameworkID fid;
      tie(fid) = unpack<F2M_REVIVE_OFFERS>(body());
      Framework *framework = lookupFramework(fid);
      if (framework != NULL) {
        LOG(INFO) << "Reviving offers for " << framework;
        framework->slaveFilter.clear();
        allocator->offersRevived(framework);
      }
      break;
    }

    case F2M_KILL_TASK: {
      FrameworkID fid;
      TaskID tid;
      tie(fid, tid) = unpack<F2M_KILL_TASK>(body());
      Framework *framework = lookupFramework(fid);
      if (framework != NULL) {
        Task *task = framework->lookupTask(tid);
        if (task != NULL) {
          LOG(INFO) << "Asked to kill " << task << " by its framework";
          killTask(task);
	} else {
	  LOG(INFO) << "Asked to kill UNKNOWN task by its framework";
	  send(framework->pid, pack<M2F_STATUS_UPDATE>(tid, TASK_LOST, ""));
        }
      }
      break;
    }

    case F2M_FRAMEWORK_MESSAGE: {
      FrameworkID fid;
      FrameworkMessage message;
      tie(fid, message) = unpack<F2M_FRAMEWORK_MESSAGE>(body());
      Framework *framework = lookupFramework(fid);
      if (framework != NULL) {
        Slave *slave = lookupSlave(message.slaveId);
        if (slave != NULL)
          send(slave->pid, pack<M2S_FRAMEWORK_MESSAGE>(fid, message));
      }
      break;
    }

    case S2M_REGISTER_SLAVE: {
      string slaveId = masterId + "-" + lexical_cast<string>(nextSlaveId++);
      Slave *slave = new Slave(from(), slaveId, elapsed());
      tie(slave->hostname, slave->publicDns, slave->resources) =
        unpack<S2M_REGISTER_SLAVE>(body());
      LOG(INFO) << "Registering " << slave << " at " << slave->pid;
      slaves[slave->id] = slave;
      pidToSid[slave->pid] = slave->id;
      link(slave->pid);
      send(slave->pid,
	   pack<M2S_REGISTER_REPLY>(slave->id, HEARTBEAT_INTERVAL));
      allocator->slaveAdded(slave);
      break;
    }

    case S2M_REREGISTER_SLAVE: {
      Slave *slave = new Slave(from(), "", elapsed());
      vector<Task> tasks;
      tie(slave->id, slave->hostname, slave->publicDns,
          slave->resources, tasks) = unpack<S2M_REREGISTER_SLAVE>(body());

      if (slave->id == "") {
        slave->id = masterId + "-" + lexical_cast<string>(nextSlaveId++);
        LOG(ERROR) << "Slave re-registered without a SlaveID, "
                   << "generating a new id for it.";
      }

      LOG(INFO) << "Re-registering " << slave << " at " << slave->pid;
      slaves[slave->id] = slave;
      pidToSid[slave->pid] = slave->id;
      link(slave->pid);
      send(slave->pid,
           pack<M2S_REREGISTER_REPLY>(slave->id, HEARTBEAT_INTERVAL));

      allocator->slaveAdded(slave);

      foreach (const Task &t, tasks) {
        Task *task = new Task(t);
        slave->addTask(task);

        // Tell this slave the current framework pid for this task.
        Framework *framework = lookupFramework(task->frameworkId);
        if (framework != NULL) {
          framework->addTask(task);
          send(slave->pid, pack<M2S_UPDATE_FRAMEWORK_PID>(framework->id,
                                                          framework->pid));
        }
      }

      // TODO(benh|alig): We should put a timeout on how long we keep
      // tasks running that never have frameworks reregister that
      // claim them.

      break;
    }

    case S2M_UNREGISTER_SLAVE: {
      SlaveID sid;
      tie(sid) = unpack<S2M_UNREGISTER_SLAVE>(body());
      LOG(INFO) << "Asked to unregister slave " << sid;
      Slave *slave = lookupSlave(sid);
      if (slave != NULL)
        removeSlave(slave);
      break;
    }

    case S2M_FT_STATUS_UPDATE: {
      SlaveID sid;
      FrameworkID fid;
      TaskID tid;
      TaskState state;
      string data;
      tie(sid, fid, tid, state, data) = unpack<S2M_FT_STATUS_UPDATE>(body());

      VLOG(1) << "FT: prepare relay seq:"<< seq() << " from: "<< from();
      if (Slave *slave = lookupSlave(sid)) {
        if (Framework *framework = lookupFramework(fid)) {
	  // Pass on the status update to the framework.
	  // TODO(benh): Do we not want to forward the
	  // S2M_FT_STATUS_UPDATE message? This seems a little tricky
	  // because we really wanted to send the M2F_FT_STATUS_UPDATE
	  // message.
          forward(framework->pid);
          if (duplicate()) {
            LOG(WARNING) << "FT: Locally ignoring duplicate message with id:" << seq();
            break;
          }
          // Update the task state locally.
          Task *task = slave->lookupTask(fid, tid);
          if (task != NULL) {
            LOG(INFO) << "Status update: " << task << " is in state " << state;
            task->state = state;
            // Remove the task if it finished or failed
            if (state == TASK_FINISHED || state == TASK_FAILED ||
                state == TASK_KILLED || state == TASK_LOST) {
              LOG(INFO) << "Removing " << task << " because it's done";
              removeTask(task, TRR_TASK_ENDED);
            }
          }
        } else {
          LOG(ERROR) << "S2M_FT_STATUS_UPDATE error: couldn't lookup "
                     << "framework id " << fid;
        }
      } else {
        LOG(ERROR) << "S2M_FT_STATUS_UPDATE error: couldn't lookup slave id "
                   << sid;
      }
      break;
    }

    case S2M_STATUS_UPDATE: {
      SlaveID sid;
      FrameworkID fid;
      TaskID tid;
      TaskState state;
      string data;
      tie(sid, fid, tid, state, data) = unpack<S2M_STATUS_UPDATE>(body());
      if (Slave *slave = lookupSlave(sid)) {
        if (Framework *framework = lookupFramework(fid)) {
          // Pass on the status update to the framework
          send(framework->pid, pack<M2F_STATUS_UPDATE>(tid, state, data));
          // Update the task state locally
          Task *task = slave->lookupTask(fid, tid);
          if (task != NULL) {
            LOG(INFO) << "Status update: " << task << " is in state " << state;
            task->state = state;
            // Remove the task if it finished or failed
            if (state == TASK_FINISHED || state == TASK_FAILED ||
                state == TASK_KILLED || state == TASK_LOST) {
              LOG(INFO) << "Removing " << task << " because it's done";
              removeTask(task, TRR_TASK_ENDED);
            }
          }
        } else {
          LOG(ERROR) << "S2M_STATUS_UPDATE error: couldn't lookup framework id "
                     << fid;
        }
      } else {
        LOG(ERROR) << "S2M_STATUS_UPDATE error: couldn't lookup slave id "
                   << sid;
      }
     break;
    }

    case S2M_FRAMEWORK_MESSAGE: {
      SlaveID sid;
      FrameworkID fid;
      FrameworkMessage message;
      tie(sid, fid, message) = unpack<S2M_FRAMEWORK_MESSAGE>(body());
      Slave *slave = lookupSlave(sid);
      if (slave != NULL) {
        Framework *framework = lookupFramework(fid);
        if (framework != NULL)
          send(framework->pid, pack<M2F_FRAMEWORK_MESSAGE>(message));
      }
      break;
    }

    case S2M_LOST_EXECUTOR: {
      SlaveID sid;
      FrameworkID fid;
      int32_t status;
      tie(sid, fid, status) = unpack<S2M_LOST_EXECUTOR>(body());
      Slave *slave = lookupSlave(sid);
      if (slave != NULL) {
        Framework *framework = lookupFramework(fid);
        if (framework != NULL) {
          // TODO(benh): Send the framework it's executor's exit status?
          if (status == -1) {
            LOG(INFO) << "Executor on " << slave << " (" << slave->hostname
                      << ") disconnected";
          } else {
            LOG(INFO) << "Executor on " << slave << " (" << slave->hostname
                      << ") exited with status " << status;
          }

          // Collect all the lost tasks for this framework.
          set<Task*> tasks;
          foreachpair (_, Task* task, framework->tasks)
            if (task->slaveId == slave->id)
              tasks.insert(task);

          // Tell the framework they have been lost and remove them.
          foreach (Task* task, tasks) {
            send(framework->pid, pack<M2F_STATUS_UPDATE>(task->id, TASK_LOST,
                                                         task->message));

            LOG(INFO) << "Removing " << task << " because of lost executor";
            removeTask(task, TRR_EXECUTOR_LOST);
          }

          // TODO(benh): Might we still want something like M2F_EXECUTOR_LOST?
        }
      }
      break;
    }

    case SH2M_HEARTBEAT: {
      SlaveID sid;
      tie(sid) = unpack<SH2M_HEARTBEAT>(body());
      Slave *slave = lookupSlave(sid);
      if (slave != NULL) {
        slave->lastHeartbeat = elapsed();
      } else {
        LOG(WARNING) << "Received heartbeat for UNKNOWN slave " << sid
                     << " from " << from();
      }
      break;
    }

    case M2M_TIMER_TICK: {
      unordered_map<SlaveID, Slave *> slavesCopy = slaves;
      foreachpair (_, Slave *slave, slavesCopy) {
	if (slave->lastHeartbeat + HEARTBEAT_TIMEOUT <= elapsed()) {
	  LOG(INFO) << slave << " missing heartbeats ... considering disconnected";
	  removeSlave(slave);
	}
      }

      // Check which framework filters can be expired.
      foreachpair (_, Framework *framework, frameworks)
        framework->removeExpiredFilters(elapsed());

      // Do allocations!
      allocator->timerTick();

      // int cnts = 0;
      // foreachpair(_, Framework *framework, frameworks) {
      // 	VLOG(1) << (cnts++) << " resourceInUse:" << framework->resources;
      // }
      break;
    }

    case M2M_FRAMEWORK_EXPIRED: {
      FrameworkID fid;
      tie(fid) = unpack<M2M_FRAMEWORK_EXPIRED>(body());
      if (Framework *framework = lookupFramework(fid)) {
	LOG(INFO) << "Framework failover timer expired, removing framework "
		  << framework;
	removeFramework(framework);
      }
      break;
    }

    case PROCESS_EXIT: {
      // TODO(benh): Could we get PROCESS_EXIT from a network partition?
      LOG(INFO) << "Process exited: " << from();
      if (pidToFid.find(from()) != pidToFid.end()) {
        FrameworkID fid = pidToFid[from()];
        if (Framework *framework = lookupFramework(fid)) {
          LOG(INFO) << framework << " disconnected";
//   	  framework->failoverTimer = new FrameworkFailoverTimer(self(), fid);
//   	  link(spawn(framework->failoverTimer));
	  removeFramework(framework);
        }
      } else if (pidToSid.find(from()) != pidToSid.end()) {
        SlaveID sid = pidToSid[from()];
        if (Slave *slave = lookupSlave(sid)) {
          LOG(INFO) << slave << " disconnected";
          removeSlave(slave);
        }
      } else {
	foreachpair (_, Framework *framework, frameworks) {
	  if (framework->failoverTimer != NULL &&
	      framework->failoverTimer->self() == from()) {
	    LOG(INFO) << "Lost framework failover timer, removing framework "
		      << framework;
	    removeFramework(framework);
	    break;
	  }
	}
      }
      break;
    }

    case M2M_GET_STATE: {
      send(from(), pack<M2M_GET_STATE_REPLY>(getState()));
      break;
    }
    
    case M2M_SHUTDOWN: {
      LOG(INFO) << "Asked to shut down by " << from();
      foreachpair (_, Slave *slave, slaves)
        send(slave->pid, pack<M2S_SHUTDOWN>());
      return;
    }

    default:
      LOG(ERROR) << "Received unknown MSGID " << msgid() << " from " << from();
      break;
    }
  }
}


OfferID Master::makeOffer(Framework *framework,
                          const vector<SlaveResources>& resources)
{
  OfferID oid = masterId + "-" + lexical_cast<string>(nextSlotOfferId++);

  SlotOffer *offer = new SlotOffer(oid, framework->id, resources);
  slotOffers[offer->id] = offer;
  framework->addOffer(offer);
  foreach (const SlaveResources& r, resources) {
    r.slave->slotOffers.insert(offer);
    r.slave->resourcesOffered += r.resources;
  }
  LOG(INFO) << "Sending " << offer << " to " << framework;
  vector<SlaveOffer> offers;
  map<SlaveID, PID> pids;
  foreach (const SlaveResources& r, resources) {
    Params params;
    params.set("cpus", r.resources.cpus);
    params.set("mem", r.resources.mem);
    SlaveOffer offer(r.slave->id, r.slave->hostname, params.getMap());
    offers.push_back(offer);
    pids[r.slave->id] = r.slave->pid;
  }
  send(framework->pid, pack<M2F_SLOT_OFFER>(oid, offers, pids));
  return oid;
}


// Process a resource offer reply (for a non-cancelled offer) by launching
// the desired tasks (if the offer contains a valid set of tasks) and
// reporting any unused resources to the allocator
void Master::processOfferReply(SlotOffer *offer,
    const vector<TaskDescription>& tasks, const Params& params)
{
  LOG(INFO) << "Received reply for " << offer;

  Framework *framework = lookupFramework(offer->frameworkId);
  CHECK(framework != NULL);

  // Count resources in the offer
  unordered_map<Slave *, Resources> offerResources;
  foreach (SlaveResources &r, offer->resources) {
    offerResources[r.slave] = r.resources;
  }

  // Count resources in the response, and check that its tasks are valid
  unordered_map<Slave *, Resources> responseResources;
  foreach (const TaskDescription &t, tasks) {
    // Check whether this task size is valid
    Params params(t.params);
    Resources res(params.getInt32("cpus", -1),
                  params.getInt32("mem", -1));
    if (res.cpus < MIN_CPUS || res.mem < MIN_MEM || 
        res.cpus > MAX_CPUS || res.mem > MAX_MEM) {
      terminateFramework(framework, 0,
          "Invalid task size: " + lexical_cast<string>(res));
      return;
    }
    // Check whether the task is on a valid slave
    Slave *slave = lookupSlave(t.slaveId);
    if (!slave || offerResources.find(slave) == offerResources.end()) {
      terminateFramework(framework, 0, "Invalid slave in offer reply");
      return;
    }
    responseResources[slave] += res;
  }

  // Check that the total accepted on each slave isn't more than offered
  foreachpair (Slave *s, Resources& respRes, responseResources) {
    Resources &offRes = offerResources[s];
    if (respRes.cpus > offRes.cpus || respRes.mem > offRes.mem) {
      terminateFramework(framework, 0, "Too many resources accepted");
      return;
    }
  }

  // Check that there are no duplicate task IDs
  unordered_set<TaskID> idsInResponse;
  foreach (const TaskDescription &t, tasks) {
    if (framework->tasks.find(t.taskId) != framework->tasks.end() ||
        idsInResponse.find(t.taskId) != idsInResponse.end()) {
      terminateFramework(framework, 0,
          "Duplicate task ID: " + lexical_cast<string>(t.taskId));
      return;
    }
    idsInResponse.insert(t.taskId);
  }

  // Launch the tasks in the response
  foreach (const TaskDescription &t, tasks) {
    launchTask(framework, t);
  }

  // If there are resources left on some slaves, add filters for them
  vector<SlaveResources> resourcesLeft;
  int timeout = params.getInt32("timeout", DEFAULT_REFUSAL_TIMEOUT);
  double expiry = (timeout == -1) ? 0 : elapsed() + timeout;
  foreachpair (Slave *s, Resources offRes, offerResources) {
    Resources respRes = responseResources[s];
    Resources left = offRes - respRes;
    if (left.cpus > 0 || left.mem > 0) {
      resourcesLeft.push_back(SlaveResources(s, left));
    }
    if (timeout != 0 && respRes.cpus == 0 && respRes.mem == 0) {
      LOG(INFO) << "Adding filter on " << s << " to " << framework
                << " for  " << timeout << " seconds";
      framework->slaveFilter[s] = expiry;
    }
  }
  
  // Return the resources left to the allocator
  removeSlotOffer(offer, ORR_FRAMEWORK_REPLIED, resourcesLeft);
}


void Master::launchTask(Framework *framework, const TaskDescription& t)
{
  Params params(t.params);
  Resources res(params.getInt32("cpus", -1),
                params.getInt32("mem", -1));

  // The invariant right now is that launchTask is called only for
  // TaskDescriptions where the slave is still valid (see the code
  // above in processOfferReply).
  Slave *slave = lookupSlave(t.slaveId);
  CHECK(slave != NULL);

  Task *task = new Task(t.taskId, framework->id, res, TASK_STARTING,
                        t.name, "", slave->id);

  framework->addTask(task);
  slave->addTask(task);

  allocator->taskAdded(task);

  LOG(INFO) << "Launching " << task << " on " << slave;
  send(slave->pid, pack<M2S_RUN_TASK>(
        framework->id, t.taskId, framework->name, framework->user,
        framework->executorInfo, t.name, t.data, t.params, framework->pid));
}


void Master::rescindOffer(SlotOffer *offer)
{
  removeSlotOffer(offer, ORR_OFFER_RESCINDED, offer->resources);
}


void Master::killTask(Task *task)
{
  LOG(INFO) << "Killing " << task;
  Framework *framework = lookupFramework(task->frameworkId);
  Slave *slave = lookupSlave(task->slaveId);
  CHECK(framework != NULL);
  CHECK(slave != NULL);
  send(slave->pid, pack<M2S_KILL_TASK>(framework->id, task->id));
}


// Terminate a framework, sending it a particular error message
// TODO: Make the error codes and messages programmer-friendly
void Master::terminateFramework(Framework *framework,
                                int32_t code,
                                const std::string& message)
{
  LOG(INFO) << "Terminating " << framework << " due to error: " << message;
  send(framework->pid, pack<M2F_ERROR>(code, message));
  removeFramework(framework);
}


// Remove a slot offer (because it was replied or we lost a framework or slave)
void Master::removeSlotOffer(SlotOffer *offer,
                             OfferReturnReason reason,
                             const vector<SlaveResources>& resourcesLeft)
{
  // Remove from slaves
  foreach (SlaveResources& r, offer->resources) {
    CHECK(r.slave != NULL);
    r.slave->resourcesOffered -= r.resources;
    r.slave->slotOffers.erase(offer);
  }
    
  // Remove from framework
  Framework *framework = lookupFramework(offer->frameworkId);
  CHECK(framework != NULL);
  framework->removeOffer(offer);
  // Also send framework a rescind message unless the reason we are
  // removing the offer is that the framework replied to it
  if (reason != ORR_FRAMEWORK_REPLIED) {
    send(framework->pid, pack<M2F_RESCIND_OFFER>(offer->id));
  }
  
  // Tell the allocator about the resources freed up
  allocator->offerReturned(offer, reason, resourcesLeft);
  
  // Delete it
  slotOffers.erase(offer->id);
  delete offer;
}


void Master::addFramework(Framework *framework)
{
  CHECK(frameworks.find(framework->id) == frameworks.end());

  frameworks[framework->id] = framework;
  pidToFid[framework->pid] = framework->id;
  link(framework->pid);

  send(framework->pid, pack<M2F_REGISTER_REPLY>(framework->id));

  allocator->frameworkAdded(framework);
}


void Master::replaceFramework(Framework *old, Framework *current)
{
  CHECK(old->id == current->id);

  old->active = false;
  
  // Remove the framework's slot offers.
  // TODO(benh): Consider just reoffering these to the new framework.
  unordered_set<SlotOffer *> slotOffersCopy = old->slotOffers;
  foreach (SlotOffer* offer, slotOffersCopy) {
    removeSlotOffer(offer, ORR_FRAMEWORK_FAILOVER, offer->resources);
  }

  send(old->pid, pack<M2F_ERROR>(1, "Framework failover"));

  // TODO(benh): unlink(old->pid);
  pidToFid.erase(old->pid);
  delete old;

  frameworks[current->id] = current;
  pidToFid[current->pid] = current->id;
  link(current->pid);

  send(current->pid, pack<M2F_REGISTER_REPLY>(current->id));
}


// Kill all of a framework's tasks, delete the framework object, and
// reschedule slot offers for slots that were assigned to this framework
void Master::removeFramework(Framework *framework)
{ 
  framework->active = false;
  // TODO: Notify allocator that a framework removal is beginning?
  
  // Tell slaves to kill the framework
  foreachpair (_, Slave *slave, slaves)
    send(slave->pid, pack<M2S_KILL_FRAMEWORK>(framework->id));

  // Remove pointers to the framework's tasks in slaves
  unordered_map<TaskID, Task *> tasksCopy = framework->tasks;
  foreachpair (_, Task *task, tasksCopy) {
    Slave *slave = lookupSlave(task->slaveId);
    CHECK(slave != NULL);
    removeTask(task, TRR_FRAMEWORK_LOST);
  }
  
  // Remove the framework's slot offers
  unordered_set<SlotOffer *> slotOffersCopy = framework->slotOffers;
  foreach (SlotOffer* offer, slotOffersCopy) {
    removeSlotOffer(offer, ORR_FRAMEWORK_LOST, offer->resources);
  }

  // TODO(benh): Similar code between removeFramework and
  // replaceFramework needs to be shared!

  // TODO(benh): unlink(framework->pid);
  pidToFid.erase(framework->pid);

  // Delete it
  frameworks.erase(framework->id);
  allocator->frameworkRemoved(framework);
  delete framework;
}


// Lose all of a slave's tasks and delete the slave object
void Master::removeSlave(Slave *slave)
{ 
  slave->active = false;
  // TODO: Notify allocator that a slave removal is beginning?
  
  // Remove pointers to slave's tasks in frameworks, and send status updates
  unordered_map<pair<FrameworkID, TaskID>, Task *> tasksCopy = slave->tasks;
  foreachpair (_, Task *task, tasksCopy) {
    Framework *framework = lookupFramework(task->frameworkId);
    // A framework might not actually exist because the master failed
    // over and the framework hasn't reconnected. This can be a tricky
    // situation for frameworks that want to have high-availability,
    // because if they eventually do connect they won't ever get a
    // status update about this task.  Perhaps in the future what we
    // want to do is create a local Framework object to represent that
    // framework until it fails over. See the TODO above in
    // S2M_REREGISTER_SLAVE.
    if (framework != NULL)
      send(framework->pid, pack<M2F_STATUS_UPDATE>(task->id, TASK_LOST,
						   task->message));
    removeTask(task, TRR_SLAVE_LOST);
  }

  // Remove slot offers from the slave; this will also rescind them
  unordered_set<SlotOffer *> slotOffersCopy = slave->slotOffers;
  foreach (SlotOffer *offer, slotOffersCopy) {
    // Only report resources on slaves other than this one to the allocator
    vector<SlaveResources> otherSlaveResources;
    foreach (SlaveResources& r, offer->resources) {
      if (r.slave != slave) {
        otherSlaveResources.push_back(r);
      }
    }
    removeSlotOffer(offer, ORR_SLAVE_LOST, otherSlaveResources);
  }
  
  // Remove slave from any filters
  foreachpair (_, Framework *framework, frameworks)
    framework->slaveFilter.erase(slave);
  
  // Send lost-slave message to all frameworks (this helps them re-run
  // previously finished tasks whose output was on the lost slave)
  foreachpair (_, Framework *framework, frameworks)
    send(framework->pid, pack<M2F_LOST_SLAVE>(slave->id));

  // TODO(benh): unlink(slave->pid);
  pidToSid.erase(slave->pid);

  // Delete it
  slaves.erase(slave->id);
  allocator->slaveRemoved(slave);
  delete slave;
}


// Remove a slot offer (because it was replied or we lost a framework or slave)
void Master::removeTask(Task *task, TaskRemovalReason reason)
{
  Framework *framework = lookupFramework(task->frameworkId);
  Slave *slave = lookupSlave(task->slaveId);
  CHECK(framework != NULL);
  CHECK(slave != NULL);
  framework->removeTask(task->id);
  slave->removeTask(task);
  allocator->taskRemoved(task, reason);
  delete task;
}


Allocator* Master::createAllocator()
{
  LOG(INFO) << "Creating \"" << allocatorType << "\" allocator";
  return AllocatorFactory::instantiate(allocatorType, this);
}


// Create a new framework ID. We format the ID as MASTERID-FWID, where
// MASTERID is the ID of the master (launch date plus fault tolerant ID)
// and FWID is an increasing integer.
FrameworkID Master::newFrameworkId()
{
  int fwId = nextFrameworkId++;
  ostringstream oss;
  oss << masterId << "-" << setw(4) << setfill('0') << fwId;
  return oss.str();
}


const Params& Master::getConf()
{
  return conf;
}
