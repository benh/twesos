%module(directors="1") mesos

#define SWIG_NO_EXPORT_ITERATOR_METHODS

%{
#include <algorithm>
#include <stdexcept>

#include <mesos_sched.hpp>
#include <mesos_exec.hpp>

#define SWIG_STD_NOASSIGN_STL
%}

%include <stdint.i>
%include <std_map.i>
%include <std_string.i>
%include <std_vector.i>

#ifdef SWIGPYTHON
  /* Add a reference to scheduler in the Python wrapper object to prevent it
     from being garbage-collected while the MesosSchedulerDriver exists */
  %feature("pythonappend") mesos::MesosSchedulerDriver::MesosSchedulerDriver %{
        self.scheduler = args[0]
  %}

  /* Add a reference to executor in the Python wrapper object to prevent it
     from being garbage-collected while the MesosExecutorDriver exists */
  %feature("pythonappend") mesos::MesosExecutorDriver::MesosExecutorDriver %{
        self.executor = args[0]
  %}

  /* Declare template instantiations we will use */
  %template(SlaveOfferVector) std::vector<mesos::SlaveOffer>;
  %template(TaskDescriptionVector) std::vector<mesos::TaskDescription>;
  %template(StringMap) std::map<std::string, std::string>;
#endif /* SWIGPYTHON */

/* Rename task_state enum so that the generated class is called TaskState */
%rename(TaskState) task_state;

/* Make it possible to inherit from Scheduler/Executor in target language */
%feature("director") mesos::Scheduler;
%feature("director") mesos::Executor;

%include <mesos_types.h>
%include <mesos_types.hpp>
%include <mesos.hpp>
%include <mesos_sched.hpp>
%include <mesos_exec.hpp>
