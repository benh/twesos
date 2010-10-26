#include <mesos_exec.h>
#include <signal.h>

#include <cerrno>
#include <iostream>
#include <string>
#include <sstream>

#include <mesos_exec.hpp>
#include <process.hpp>

#include <boost/bind.hpp>
#include <boost/unordered_map.hpp>

#include "common/fatal.hpp"
#include "common/lock.hpp"
#include "common/logging.hpp"

#include "messaging/messages.hpp"

using std::cerr;
using std::endl;
using std::string;

using boost::bind;
using boost::ref;
using boost::unordered_map;

using namespace mesos;
using namespace mesos::internal;


namespace mesos { namespace internal {

class ExecutorProcess : public MesosProcess
{
public:
  friend class mesos::MesosExecutorDriver;
  
protected:
  PID slave;
  MesosExecutorDriver* driver;
  Executor* executor;
  FrameworkID fid;
  SlaveID sid;
  bool local;

  volatile bool terminate;

public:
  ExecutorProcess(const PID& _slave,
                  MesosExecutorDriver* _driver,
                  Executor* _executor,
                  FrameworkID _fid,
                  bool _local)
    : slave(_slave), driver(_driver), executor(_executor),
      fid(_fid), local(_local), terminate(false) {}

protected:
  void operator() ()
  {
    link(slave);
    send(slave, pack<E2S_REGISTER_EXECUTOR>(fid));
    while(true) {
      // TODO(benh): Is there a better way to architect this code? In
      // particular, if the executor blocks in a callback, we can't
      // process any other messages. This is especially tricky if a
      // slave dies since we won't handle the PROCESS_EXIT message in
      // a timely manner (if at all).

      // Check for terminate in the same way as SchedulerProcess. See
      // comments there for an explanation of why this is necessary.
      if (terminate)
        return;

      switch(receive(2)) {
        case S2E_REGISTER_REPLY: {
          string host;
          string fwName;
          string args;
          tie(sid, host, fwName, args) = unpack<S2E_REGISTER_REPLY>(body());
          ExecutorArgs execArg(sid, host, fid, fwName, args);
          invoke(bind(&Executor::init, executor, driver, ref(execArg)));
          break;
        }

        case S2E_RUN_TASK: {
          TaskID tid;
          string name;
          string args;
          Params params;
          tie(tid, name, args, params) = unpack<S2E_RUN_TASK>(body());
          TaskDescription task(tid, sid, name, params.getMap(), args);
          send(slave, pack<E2S_STATUS_UPDATE>(fid, tid, TASK_RUNNING, ""));
          invoke(bind(&Executor::launchTask, executor, driver, ref(task)));
          break;
        }

        case S2E_KILL_TASK: {
          TaskID tid;
          tie(tid) = unpack<S2E_KILL_TASK>(body());
          invoke(bind(&Executor::killTask, executor, driver, tid));
          break;
        }

        case S2E_FRAMEWORK_MESSAGE: {
          FrameworkMessage msg;
          tie(msg) = unpack<S2E_FRAMEWORK_MESSAGE>(body());
          invoke(bind(&Executor::frameworkMessage, executor, driver, ref(msg)));
          break;
        }

        case S2E_KILL_EXECUTOR: {
          invoke(bind(&Executor::shutdown, executor, driver));
          if (!local)
            exit(0);
          else
            return;
        }

        case PROCESS_EXIT: {
          // TODO: Pass an argument to shutdown to tell it this is abnormal?
          invoke(bind(&Executor::shutdown, executor, driver));

          // This is a pretty bad state ... no slave is left. Rather
          // than exit lets kill our process group (which includes
          // ourself) hoping to clean up any processes this executor
          // launched itself.
          // TODO(benh): Maybe do a SIGTERM and then later do a SIGKILL?
          if (!local)
            killpg(0, SIGKILL);
          else
            return;
        }

        case PROCESS_TIMEOUT: {
          break;
        }

        default: {
          // TODO: Is this serious enough to exit?
          cerr << "Received unknown message ID " << msgid()
               << " from " << from() << endl;
          break;
        }
      }
    }
  }
};

}} /* namespace mesos { namespace internal { */


/*
 * Implementation of C++ API.
 */


// Default implementation of error() that logs to stderr and exits
void Executor::error(ExecutorDriver* driver, int code, const string &message)
{
  cerr << "Mesos error: " << message
       << " (error code: " << code << ")" << endl;
  driver->stop();
}


MesosExecutorDriver::MesosExecutorDriver(Executor* _executor)
  : executor(_executor), running(false)
{
  // Create mutex and condition variable
  pthread_mutexattr_t attr;
  pthread_mutexattr_init(&attr);
  pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
  pthread_mutex_init(&mutex, &attr);
  pthread_mutexattr_destroy(&attr);
  pthread_cond_init(&cond, 0);
}


MesosExecutorDriver::~MesosExecutorDriver()
{
  Process::wait(process->self());
  delete process;

  pthread_mutex_destroy(&mutex);
  pthread_cond_destroy(&cond);
}


int MesosExecutorDriver::start()
{
  Lock lock(&mutex);

  if (running) {
    return -1;
  }

  // Set stream buffering mode to flush on newlines so that we capture logs
  // from user processes even when output is redirected to a file.
  setvbuf(stdout, 0, _IOLBF, 0);
  setvbuf(stderr, 0, _IOLBF, 0);

  bool local;

  PID slave;
  FrameworkID fid;

  char* value;
  std::istringstream iss;

  /* Check if this is local (for example, for testing). */
  value = getenv("MESOS_LOCAL");

  if (value != NULL)
    local = true;
  else
    local = false;

  /* Get slave PID from environment. */
  value = getenv("MESOS_SLAVE_PID");

  if (value == NULL)
    fatal("expecting MESOS_SLAVE_PID in environment");

  slave = PID(value);

  if (!slave)
    fatal("cannot parse MESOS_SLAVE_PID");

  /* Get framework ID from environment. */
  value = getenv("MESOS_FRAMEWORK_ID");

  if (value == NULL)
    fatal("expecting MESOS_FRAMEWORK_ID in environment");

  iss.str(value);

  if (!(iss >> fid))
    fatal("cannot parse MESOS_FRAMEWORK_ID");

  process = new ExecutorProcess(slave, this, executor, fid, local);

  Process::spawn(process);

  running = true;

  return 0;
}


int MesosExecutorDriver::stop()
{
  Lock lock(&mutex);

  if (!running) {
    return -1;
  }

  process->terminate = true;

  running = false;

  pthread_cond_signal(&cond);

  return 0;
}


int MesosExecutorDriver::join()
{
  Lock lock(&mutex);
  while (running)
    pthread_cond_wait(&cond, &mutex);

  return 0;
}


int MesosExecutorDriver::run()
{
  int ret = start();
  return ret != 0 ? ret : join();
}


int MesosExecutorDriver::sendStatusUpdate(const TaskStatus &status)
{
  Lock lock(&mutex);

  if (!running) {
    //executor->error(this, EINVAL, "Executor has exited");
    return -1;
  }

  process->send(process->slave,
                pack<E2S_STATUS_UPDATE>(process->fid,
                                        status.taskId,
                                        status.state,
                                        status.data));

  return 0;
}


int MesosExecutorDriver::sendFrameworkMessage(const FrameworkMessage &message)
{
  Lock lock(&mutex);

  if (!running) {
    //executor->error(this, EINVAL, "Executor has exited");
    return -1;
  }

  process->send(process->slave,
                pack<E2S_FRAMEWORK_MESSAGE>(process->fid, message));

  return 0;
}


/*
 * Implementation of C API.
 */


namespace mesos { namespace internal {

/*
 * We wrap calls from the C API into the C++ API with the following
 * specialized implementation of Executor.
 */
class CExecutor : public Executor {
public:
  mesos_exec* exec;
  ExecutorDriver* driver; // Set externally after object is created
  
  CExecutor(mesos_exec* _exec) : exec(_exec), driver(NULL) {}

  virtual ~CExecutor() {}

  virtual void init(ExecutorDriver*, const ExecutorArgs& args)
  {
    exec->init(exec,
               args.slaveId.c_str(),
               args.host.c_str(),
               args.frameworkId.c_str(),
               args.frameworkName.c_str(),
               args.data.data(),
               args.data.size());
  }

  virtual void launchTask(ExecutorDriver*, const TaskDescription& task)
  {
    // Convert params to key=value list
    Params paramsObj(task.params);
    string paramsStr = paramsObj.str();
    mesos_task_desc td = { task.taskId,
                           task.slaveId.c_str(),
                           task.name.c_str(),
                           paramsStr.c_str(),
                           task.data.data(),
                           task.data.size() };
    exec->launch_task(exec, &td);
  }

  virtual void killTask(ExecutorDriver*, TaskID taskId)
  {
    exec->kill_task(exec, taskId);
  }
  
  virtual void frameworkMessage(ExecutorDriver*,
                                const FrameworkMessage& message)
  {
    mesos_framework_message msg = { message.slaveId.c_str(),
                                    message.taskId,
                                    message.data.data(),
                                    message.data.size() };
    exec->framework_message(exec, &msg);
  }
  
  virtual void shutdown(ExecutorDriver*)
  {
    exec->shutdown(exec);
  }
  
  virtual void error(ExecutorDriver*, int code, const std::string& message)
  {
    exec->error(exec, code, message.c_str());
  }
};


/*
 * A single CExecutor instance used with the C API.
 *
 * TODO: Is this a good idea? How can one unit-test C frameworks? It might
 *       be better to have a hashtable as in the scheduler API eventually.
 */
CExecutor* c_executor = NULL;

}} /* namespace mesos { namespace internal {*/


extern "C" {


int mesos_exec_run(struct mesos_exec* exec)
{
  if (exec == NULL || c_executor != NULL) {
    errno = EINVAL;
    return -1;
  }

  CExecutor executor(exec);
  c_executor = &executor;
  
  MesosExecutorDriver driver(&executor);
  executor.driver = &driver;
  driver.run();

  c_executor = NULL;

  return 0;
}


int mesos_exec_send_message(struct mesos_exec* exec,
                            struct mesos_framework_message* msg)
{
  if (exec == NULL || c_executor == NULL || msg == NULL) {
    errno = EINVAL;
    return -1;
  }

  string data((char*) msg->data, msg->data_len);
  FrameworkMessage message(string(msg->sid), msg->tid, data);

  c_executor->driver->sendFrameworkMessage(message);

  return 0;
}


int mesos_exec_status_update(struct mesos_exec* exec,
                             struct mesos_task_status* status)
{

  if (exec == NULL || c_executor == NULL || status == NULL) {
    errno = EINVAL;
    return -1;
  }

  string data((char*) status->data, status->data_len);
  TaskStatus ts(status->tid, status->state, data);

  c_executor->driver->sendStatusUpdate(ts);

  return 0;
}

} /* extern "C" */
