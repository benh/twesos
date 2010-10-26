#ifndef MESOS_SCHED_HPP
#define MESOS_SCHED_HPP

#include <mesos.hpp>

#include <string>
#include <vector>


namespace mesos {

class SchedulerDriver;

namespace internal {
class SchedulerProcess;
class MasterDetector;
class Params;
}


/**
 * Callback interface to be implemented by new frameworks' schedulers.
 */
class Scheduler
{
public:
  virtual ~Scheduler() {}

  // Callbacks for getting framework properties
  virtual std::string getFrameworkName(SchedulerDriver* d);
  virtual ExecutorInfo getExecutorInfo(SchedulerDriver* d);

  // Callbacks for various Mesos events
  virtual void registered(SchedulerDriver* d, FrameworkID frameworkId) {}
  virtual void resourceOffer(SchedulerDriver* d,
                             OfferID offerId,
                             const std::vector<SlaveOffer>& offers) {}
  virtual void offerRescinded(SchedulerDriver* d, OfferID offerId) {}
  virtual void statusUpdate(SchedulerDriver* d, const TaskStatus& status) {}
  virtual void frameworkMessage(SchedulerDriver* d,
                                const FrameworkMessage& message) {}
  virtual void slaveLost(SchedulerDriver* d, SlaveID slaveId) {}
  virtual void error(SchedulerDriver* d, int code, const std::string& message);
};


/**
 * Abstract interface for driving a scheduler connected to Mesos.
 * This interface is used both to manage the scheduler's lifecycle (start it,
 * stop it, or wait for it to finish) and to send commands from the user
 * framework to Mesos (such as replies to offers). Concrete implementations
 * of SchedulerDriver will take a Scheduler as a parameter in order to make
 * callbacks into it on various events.
 */
class SchedulerDriver
{
public:
  virtual ~SchedulerDriver() {}

  // Lifecycle methods
  virtual int start() { return -1; }
  virtual int stop() { return -1; }
  virtual int join() { return -1; }
  virtual int run() { return -1; } // Start and then join driver

  // Communication methods
  virtual int sendFrameworkMessage(const FrameworkMessage& message) { return -1; }
  virtual int killTask(TaskID taskId) { return -1; }
  virtual int replyToOffer(OfferID offerId,
			   const std::vector<TaskDescription>& tasks,
			   const std::map<std::string, std::string>& params) { return -1; }
  virtual int replyToOffer(OfferID offerId,
                          const std::vector<TaskDescription>& tasks) { return -1; }
  virtual int reviveOffers() { return -1; }
  virtual int sendHints(const std::map<std::string, std::string>& hints) { return -1; }
};


/**
 * Concrete implementation of SchedulerDriver that communicates with
 * a Mesos master.
 */
class MesosSchedulerDriver : public SchedulerDriver
{
public:
  /**
   * Create a scheduler driver with a given Mesos master URL.
   * Additional Mesos config options are read from the environment, as
   * well as any config files found through it. If a framework ID is
   * set than consider this scheduler a failover.
   *
   * @param sched scheduler to make callbacks into
   * @param url Mesos master URL
   * @param frameworkId optional framework ID for scheduler failover
   */
  MesosSchedulerDriver(Scheduler* sched,
		       const std::string& url,
		       FrameworkID frameworkId = "");

  /**
   * Create a scheduler driver with a configuration, which the master
   * URL and possibly other options are read from.  Additional Mesos
   * config options are read from the environment, as well as any
   * config files given through conf or found in the environment. If a
   * framework ID is set than consider this scheduler a failover.
   *
   * @param sched scheduler to make callbacks into
   * @param params Map containing configuration options
   * @param frameworkId optional framework ID for scheduler failover
   */
  MesosSchedulerDriver(Scheduler* sched,
		       const std::map<std::string, std::string>& params,
		       FrameworkID frameworkId = "");

#ifndef SWIG
  /**
   * Create a scheduler driver with a config read from command-line
   * arguments.  Additional Mesos config options are read from the
   * environment, as well as any config files given through conf or
   * found in the environment. If a framework ID is set than consider
   * this scheduler a failover.
   *
   * This constructor is not available through SWIG since it's
   * difficult for it to properly map arrays to an argc/argv pair.
   *
   * @param sched scheduler to make callbacks into
   * @param argc argument count
   * @param argv argument values (argument 0 is expected to be program name
   *             and will not be looked at for options)
   * @param frameworkId optional framework ID for scheduler failover
   */
  MesosSchedulerDriver(Scheduler* sched,
		       int argc,
                       char** argv,
		       FrameworkID frameworkId = "");
#endif

  virtual ~MesosSchedulerDriver();

  // Lifecycle methods
  virtual int start();
  virtual int stop();
  virtual int join();
  virtual int run(); // Start and then join driver

  // Communication methods
  virtual int sendFrameworkMessage(const FrameworkMessage& message);
  virtual int killTask(TaskID taskId);
  virtual int replyToOffer(OfferID offerId,
			   const std::vector<TaskDescription>& tasks,
			   const std::map<std::string, std::string>& params);
  virtual int replyToOffer(OfferID offerId,
                           const std::vector<TaskDescription>& tasks) {
    return replyToOffer(offerId, tasks, std::map<std::string, std::string>());
  }
  virtual int reviveOffers();
  virtual int sendHints(const std::map<std::string, std::string>& hints);

private:
  // Initialization method used by constructors
  void init(Scheduler* sched, internal::Params* conf, FrameworkID frameworkId);

  // Internal utility method to report an error to the scheduler
  void error(int code, const std::string& message);

  Scheduler* sched;
  std::string url;
  FrameworkID frameworkId;

  // LibProcess process for communicating with master
  internal::SchedulerProcess* process;

  // Coordination between masters
  internal::MasterDetector* detector;

  // Configuration options. We're using a pointer here because we don't
  // want to #include params.hpp into the public API.
  internal::Params* conf;

  // Are we currently registered with the master
  bool running;
  
  // Mutex to enforce all non-callbacks are execute serially
  pthread_mutex_t mutex;

  // Condition variable for waiting until driver terminates
  pthread_cond_t cond;
};


} /* namespace mesos { */

#endif /* MESOS_SCHED_HPP */
