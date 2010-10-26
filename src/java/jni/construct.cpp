#include <map>
#include <string>

#include <jni.h>

#include "construct.hpp"
#include "fatal.hpp"
#include "foreach.hpp"
#include "mesos_exec.hpp"
#include "mesos_sched.hpp"

using std::map;
using std::string;

using namespace mesos;


template <>
string construct(JNIEnv *env, jobject jobj)
{
  string s;
  jstring jstr = (jstring) jobj;
  const char *str = (const char *) env->GetStringUTFChars(jstr, NULL);
  s = str;
  env->ReleaseStringUTFChars(jstr, str);
  return s;
}


template <>
bytes construct(JNIEnv *env, jobject jobj)
{
  jbyteArray jarray = (jbyteArray) jobj;
  const jsize length = env->GetArrayLength(jarray);
  jbyte *array = env->GetByteArrayElements(jarray, NULL);
  bytes data(array, length);
  env->ReleaseByteArrayElements(jarray, array, NULL);
  return data;
}


template <>
map<string, string> construct(JNIEnv *env, jobject jobj)
{
  map<string, string> result;

  jclass clazz = env->GetObjectClass(jobj);

  // Set entrySet = map.entrySet();
  jmethodID entrySet = env->GetMethodID(clazz, "entrySet", "()Ljava/util/Set;");
  jobject jentrySet = env->CallObjectMethod(jobj, entrySet);

  clazz = env->GetObjectClass(jentrySet);

  // Iterator iterator = entrySet.iterator();
  jmethodID iterator = env->GetMethodID(clazz, "iterator", "()Ljava/util/Iterator;");
  jobject jiterator = env->CallObjectMethod(jentrySet, iterator);

  clazz = env->GetObjectClass(jiterator);

  // while (iterator.hasNext()) {
  jmethodID hasNext = env->GetMethodID(clazz, "hasNext", "()Z");

  jmethodID next = env->GetMethodID(clazz, "next", "()Ljava/lang/Object;");

  while (env->CallBooleanMethod(jiterator, hasNext)) {
    // Map.Entry entry = iterator.next();
    jobject jentry = env->CallObjectMethod(jiterator, next);

    clazz = env->GetObjectClass(jentry);

    // String key = entry.getKey();
    jmethodID getKey = env->GetMethodID(clazz, "getKey", "()Ljava/lang/Object;");
    jobject jkey = env->CallObjectMethod(jentry, getKey);

    // String value = entry.getValue();
    jmethodID getValue = env->GetMethodID(clazz, "getValue", "()Ljava/lang/Object;");
    jobject jvalue = env->CallObjectMethod(jentry, getValue);

    const string& key = construct<string>(env, jkey);
    const string& value = construct<string>(env, jvalue);

    result[key] = value;
  }

  return result;
}


template <>
FrameworkID construct(JNIEnv *env, jobject jobj)
{
  jclass clazz = env->GetObjectClass(jobj);
  jfieldID id = env->GetFieldID(clazz, "s", "Ljava/lang/String;");
  jstring jstr = (jstring) env->GetObjectField(jobj, id);
  return FrameworkID(construct<string>(env, jstr));
}


template <>
TaskID construct(JNIEnv *env, jobject jobj)
{
  jclass clazz = env->GetObjectClass(jobj);
  jfieldID id = env->GetFieldID(clazz, "i", "I");
  jint jtaskId = env->GetIntField(jobj, id);
  TaskID taskId = jtaskId;
  return taskId;
}


template <>
SlaveID construct(JNIEnv *env, jobject jobj)
{
  jclass clazz = env->GetObjectClass(jobj);
  jfieldID id = env->GetFieldID(clazz, "s", "Ljava/lang/String;");
  jstring jstr = (jstring) env->GetObjectField(jobj, id);
  return SlaveID(construct<string>(env, jstr));
}


template <>
OfferID construct(JNIEnv *env, jobject jobj)
{
  jclass clazz = env->GetObjectClass(jobj);
  jfieldID id = env->GetFieldID(clazz, "s", "Ljava/lang/String;");
  jstring jstr = (jstring) env->GetObjectField(jobj, id);
  return OfferID(construct<string>(env, jstr));
}


template <>
TaskState construct(JNIEnv *env, jobject jobj)
{
  jclass clazz = env->GetObjectClass(jobj);

  jmethodID name = env->GetMethodID(clazz, "name", "()Ljava/lang/String;");
  jstring jstr = (jstring) env->CallObjectMethod(jobj, name);

  const string& str = construct<string>(env, jstr);

  if (str == "TASK_STARTING") {
    return TASK_STARTING;
  } else if (str == "TASK_RUNNING") {
    return TASK_RUNNING;
  } else if (str == "TASK_FINISHED") {
    return TASK_FINISHED;
  } else if (str == "TASK_FAILED") {
    return TASK_FAILED;
  } else if (str == "TASK_KILLED") {
    return TASK_KILLED;
  } else if (str == "TASK_LOST") {
    return TASK_LOST;
  }

  fatal("Bad enum value while converting between Java and C++.");
}


template <>
TaskDescription construct(JNIEnv *env, jobject jobj)
{
  TaskDescription desc;

  jclass clazz = env->GetObjectClass(jobj);

  jfieldID taskId = env->GetFieldID(clazz, "taskId", "Lmesos/TaskID;");
  jobject jtaskId = env->GetObjectField(jobj, taskId);
  desc.taskId = construct<TaskID>(env, jtaskId);

  jfieldID slaveId = env->GetFieldID(clazz, "slaveId", "Lmesos/SlaveID;");
  jobject jslaveId = env->GetObjectField(jobj, slaveId);
  desc.slaveId = construct<SlaveID>(env, jslaveId);

  jfieldID name = env->GetFieldID(clazz, "name", "Ljava/lang/String;");
  jstring jstr = (jstring) env->GetObjectField(jobj, name);
  desc.name = construct<string>(env, jstr);

  jfieldID params = env->GetFieldID(clazz, "params", "Ljava/util/Map;");
  jobject jparams = env->GetObjectField(jobj, params);
  desc.params = construct< map<string, string> >(env, jparams);

  jfieldID data = env->GetFieldID(clazz, "data", "[B");
  jobject jdata = env->GetObjectField(jobj, data);
  desc.data = construct<bytes>(env, (jbyteArray) jdata);

  return desc;
}


template <>
TaskStatus construct(JNIEnv *env, jobject jobj)
{
  TaskStatus status;

  jclass clazz = env->GetObjectClass(jobj);

  jfieldID taskId = env->GetFieldID(clazz, "taskId", "Lmesos/TaskID;");
  jobject jtaskId = env->GetObjectField(jobj, taskId);
  status.taskId = construct<TaskID>(env, jtaskId);

  jfieldID state = env->GetFieldID(clazz, "state", "Lmesos/TaskState;");
  jobject jstate = env->GetObjectField(jobj, state);
  status.state = construct<TaskState>(env, jstate);

  jfieldID data = env->GetFieldID(clazz, "data", "[B");
  jobject jdata = env->GetObjectField(jobj, data);
  status.data = construct<bytes>(env, jdata);

  return status;
}


template <>
FrameworkMessage construct(JNIEnv *env, jobject jobj)
{
  FrameworkMessage message;

  jclass clazz = env->GetObjectClass(jobj);

  jfieldID slaveId = env->GetFieldID(clazz, "slaveId", "Lmesos/SlaveID;");
  jobject jslaveId = env->GetObjectField(jobj, slaveId);
  message.slaveId = construct<SlaveID>(env, jslaveId);

  jfieldID taskId = env->GetFieldID(clazz, "taskId", "Lmesos/TaskID;");
  jobject jtaskId = env->GetObjectField(jobj, taskId);
  message.taskId = construct<TaskID>(env, jtaskId);

  jfieldID data = env->GetFieldID(clazz, "data", "[B");
  jobject jdata = env->GetObjectField(jobj, data);
  message.data = construct<bytes>(env, jdata);

  return message;
}


template <>
ExecutorInfo construct(JNIEnv *env, jobject jobj)
{
  ExecutorInfo execInfo;

  jclass clazz = env->GetObjectClass(jobj);

  jfieldID uri = env->GetFieldID(clazz, "uri", "Ljava/lang/String;");
  jobject juri = env->GetObjectField(jobj, uri);
  execInfo.uri = construct<string>(env, (jstring) juri);

  jfieldID data = env->GetFieldID(clazz, "data", "[B");
  jobject jdata = env->GetObjectField(jobj, data);
  execInfo.data = construct<bytes>(env, jdata);

  jfieldID params = env->GetFieldID(clazz, "params", "Ljava/util/Map;");
  jobject jparams = env->GetObjectField(jobj, params);
  execInfo.params = construct< map<string, string> >(env, jparams);

  return execInfo;
}
