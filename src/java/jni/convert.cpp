#include <map>
#include <string>

#include <jni.h>

#include "convert.hpp"
#include "fatal.hpp"
#include "foreach.hpp"
#include "mesos_exec.hpp"
#include "mesos_sched.hpp"

using std::map;
using std::string;

using namespace mesos;


template <>
jobject convert(JNIEnv *env, const string &s)
{
  return env->NewStringUTF(s.c_str());
}


template <>
jobject convert(JNIEnv *env, const bytes &data)
{
  jbyteArray jarray = env->NewByteArray(data.size());
  env->SetByteArrayRegion(jarray, 0, data.size(), (jbyte *) data.data());
  return jarray;
}


template <>
jobject convert(JNIEnv *env, const map<string, string> &m)
{
  // HashMap m = new HashMap();
  jclass clazz = env->FindClass("java/util/HashMap");

  jmethodID _init_ = env->GetMethodID(clazz, "<init>", "()V");
  jobject jm = env->NewObject(clazz, _init_);

  jmethodID put = env->GetMethodID(clazz, "put",
    "(Ljava/lang/Object;Ljava/lang/Object;)Ljava/lang/Object;");
    
  // Loop through C++ map and add each pair to the Java map.
  foreachpair (const string &key, const string &value, m) {
    jobject jkey = convert<string>(env, key);
    jobject jvalue = convert<string>(env, value);
    env->CallObjectMethod(jm, put, jkey, jvalue);
  }

  return jm;
}


template <>
jobject convert(JNIEnv *env, const FrameworkID &frameworkId)
{
  // String id = ...;
  jobject jid = convert<string>(env, frameworkId);

  // FrameworkID frameworkId = new FrameworkID(id);
  jclass clazz = env->FindClass("mesos/FrameworkID");

  jmethodID _init_ =
    env->GetMethodID(clazz, "<init>", "(Ljava/lang/String;)V");

  jobject jframeworkId = env->NewObject(clazz, _init_, jid);

  return jframeworkId;
}


template <>
jobject convert(JNIEnv *env, const TaskID &taskId)
{
  // int id = ...;
  jint jid = taskId;

  // TaskID taskId = new TaskID(id);
  jclass clazz = env->FindClass("mesos/TaskID");

  jmethodID _init_ = env->GetMethodID(clazz, "<init>", "(I)V");

  jobject jtaskId = env->NewObject(clazz, _init_, jid);

  return jtaskId;
}


template <>
jobject convert(JNIEnv *env, const SlaveID &slaveId)
{
  // String id = ...;
  jobject jid = convert<string>(env, slaveId);

  // SlaveID taskId = new SlaveID(id);
  jclass clazz = env->FindClass("mesos/SlaveID");

  jmethodID _init_ =
    env->GetMethodID(clazz, "<init>", "(Ljava/lang/String;)V");

  jobject jslaveId = env->NewObject(clazz, _init_, jid);

  return jslaveId;
}


template <>
jobject convert(JNIEnv *env, const OfferID &offerId)
{
  // String id = ...;
  jobject jid = convert<string>(env, offerId);

  // OfferID taskId = new OfferID(id);
  jclass clazz = env->FindClass("mesos/OfferID");

  jmethodID _init_ =
    env->GetMethodID(clazz, "<init>", "(Ljava/lang/String;)V");

  jobject jofferId = env->NewObject(clazz, _init_, jid);

  return jofferId;
}


template <>
jobject convert(JNIEnv *env, const TaskState &state)
{
  jclass clazz = env->FindClass("mesos/TaskState");

  const char *name = NULL;

  if (state == TASK_STARTING) {
    name = "TASK_STARTING";
  } else if (state == TASK_RUNNING) {
    name = "TASK_RUNNING";
  } else if (state == TASK_FINISHED) {
    name = "TASK_FINISHED";
  } else if (state == TASK_FAILED) {
    name = "TASK_FAILED";
  } else if (state == TASK_KILLED) {
    name = "TASK_KILLED";
  } else if (state == TASK_LOST) {
    name = "TASK_LOST";
  } else {
    fatal("bad enum value while converting between C++ and Java.");
  }

  jfieldID TASK_XXXX = env->GetStaticFieldID(clazz, name, "Lmesos/TaskState;");
  return env->GetStaticObjectField(clazz, TASK_XXXX);
}


template <>
jobject convert(JNIEnv *env, const TaskDescription &desc)
{
  jobject jtaskId = convert<TaskID>(env, desc.taskId);
  jobject jslaveId = convert<SlaveID>(env, desc.slaveId);
  jobject jname = convert<string>(env, desc.name);
  jobject jparams = convert< map<string, string> >(env, desc.params);
  jobject jdata = convert<bytes>(env, desc.data);

  // ... desc = TaskDescription(taskId, slaveId, params, data);
  jclass clazz = env->FindClass("mesos/TaskDescription");

  jmethodID _init_ = env->GetMethodID(clazz, "<init>",
    "(Lmesos/TaskID;Lmesos/SlaveID;Ljava/lang/String;Ljava/util/Map;[B)V");

  jobject jdesc = env->NewObject(clazz, _init_, jtaskId, jslaveId,
                                 jname, jparams, jdata);

  return jdesc;
}


template <>
jobject convert(JNIEnv *env, const TaskStatus &status)
{
  jobject jtaskId = convert<TaskID>(env, status.taskId);
  jobject jstate = convert<TaskState>(env, status.state);
  jobject jdata = convert<bytes>(env, status.data);

  // ... status = TaskStatus(taskId, state, data);
  jclass clazz = env->FindClass("mesos/TaskStatus");

  jmethodID _init_ = env->GetMethodID(clazz, "<init>",
    "(Lmesos/TaskID;Lmesos/TaskState;[B)V");

  jobject jstatus = env->NewObject(clazz, _init_, jtaskId, jstate, jdata);

  return jstatus;
}


template <>
jobject convert(JNIEnv *env, const SlaveOffer &offer)
{
  jobject jslaveId = convert<SlaveID>(env, offer.slaveId);
  jobject jhost = convert<string>(env, offer.host);
  jobject jparams = convert< map<string, string> >(env, offer.params);

  // SlaveOffer offer = new SlaveOffer(slaveId, host, params);
  jclass clazz = env->FindClass("mesos/SlaveOffer");

  jmethodID _init_ = env->GetMethodID(clazz, "<init>",
    "(Lmesos/SlaveID;Ljava/lang/String;Ljava/util/Map;)V");

  jobject joffer = env->NewObject(clazz, _init_, jslaveId, jhost, jparams);

  return joffer;
}


template <>
jobject convert(JNIEnv *env, const FrameworkMessage &message)
{
  jobject jslaveId = convert<SlaveID>(env, message.slaveId);
  jobject jtaskId = convert<TaskID>(env, message.taskId);
  jobject jdata = convert<bytes>(env, message.data);

  // ... message = FrameworkMessage(slaveId, taskId, data);
  jclass clazz = env->FindClass("mesos/FrameworkMessage");

  jmethodID _init_ = env->GetMethodID(clazz, "<init>",
    "(Lmesos/SlaveID;Lmesos/TaskID;[B)V");

  jobject jmessage = env->NewObject(clazz, _init_, jslaveId, jtaskId, jdata);

  return jmessage;
}


template <>
jobject convert(JNIEnv *env, const ExecutorInfo &execInfo)
{
  jobject juri = convert<string>(env, execInfo.uri);
  jobject jdata = convert<bytes>(env, execInfo.data);
  jobject jparams = convert< map<string, string> >(env, execInfo.params);

  // ... execInfo = ExecutorInfo(uri, data, params);
  jclass clazz = env->FindClass("mesos/ExecutorInfo");

  jmethodID _init_ = env->GetMethodID(clazz, "<init>",
    "(Ljava/lang/String;[BLjava/util/Map;)V");

  jobject jexecInfo = env->NewObject(clazz, _init_, juri, jdata, jparams);

  return jexecInfo;
}


template <>
jobject convert(JNIEnv *env, const ExecutorArgs &args)
{
  jobject jslaveId = convert<SlaveID>(env, args.slaveId);
  jobject jhost = convert<string>(env, args.host);
  jobject jframeworkId = convert<FrameworkID>(env, args.frameworkId);
  jobject jframeworkName = convert<string>(env, args.frameworkName);
  jobject jdata = convert<bytes>(env, args.data);

  // ... args = ExecutorArgs(slaveId, host, frameworkId, frameworkName, data);
  jclass clazz = env->FindClass("mesos/ExecutorArgs");

  jmethodID _init_ = env->GetMethodID(clazz, "<init>",
    "(Lmesos/SlaveID;Ljava/lang/String;Lmesos/FrameworkID;Ljava/lang/String;[B)V");

  jobject jargs = env->NewObject(clazz, _init_, jslaveId, jhost,
                                 jframeworkId, jframeworkName, jdata);

  return jargs;
}
