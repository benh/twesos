#include <map>
#include <string>
#include <vector>

#include "construct.hpp"
#include "convert.hpp"
#include "foreach.hpp"
#include "mesos_MesosSchedulerDriver.h"
#include "mesos_sched.hpp"

using std::map;
using std::string;
using std::vector;

using namespace mesos;


class JNIScheduler : public Scheduler
{
public:
  JNIScheduler(JNIEnv *_env, jobject _jdriver)
    : jvm(NULL), env(_env), jdriver(_jdriver)
  {
    env->GetJavaVM(&jvm);
  }

  virtual ~JNIScheduler() {}

  virtual string getFrameworkName(SchedulerDriver*);
  virtual ExecutorInfo getExecutorInfo(SchedulerDriver*);
  virtual void registered(SchedulerDriver*, FrameworkID);
  virtual void resourceOffer(SchedulerDriver*, OfferID,
                             const vector<SlaveOffer>&);
  virtual void offerRescinded(SchedulerDriver*, OfferID);
  virtual void statusUpdate(SchedulerDriver*, const TaskStatus&);
  virtual void frameworkMessage(SchedulerDriver*, const FrameworkMessage&);
  virtual void slaveLost(SchedulerDriver*, SlaveID);
  virtual void error(SchedulerDriver*, int code, const string& message);

  JavaVM *jvm;
  JNIEnv *env;
  jobject jdriver;
};


string JNIScheduler::getFrameworkName(SchedulerDriver* driver)
{
  jvm->AttachCurrentThread((void **) &env, NULL);

  string name;

  jclass clazz = env->GetObjectClass(jdriver);

  jfieldID sched = env->GetFieldID(clazz, "sched", "Lmesos/Scheduler;");
  jobject jsched = env->GetObjectField(jdriver, sched);

  clazz = env->GetObjectClass(jsched);

  // String name = sched.getFrameworkName(driver);
  jmethodID getFrameworkName =
    env->GetMethodID(clazz, "getFrameworkName", "(Lmesos/SchedulerDriver;)Ljava/lang/String;");

  env->ExceptionClear();

  jobject jname = env->CallObjectMethod(jsched, getFrameworkName, jdriver);

  if (!env->ExceptionOccurred()) {
    name = construct<string>(env, (jstring) jname);
    jvm->DetachCurrentThread();
  } else {
    env->ExceptionDescribe();
    env->ExceptionClear();
    jvm->DetachCurrentThread();
    driver->stop();
    this->error(driver, -1, "Java exception caught");
  }

  return name;
}


ExecutorInfo JNIScheduler::getExecutorInfo(SchedulerDriver* driver)
{
  jvm->AttachCurrentThread((void **) &env, NULL);

  ExecutorInfo execInfo;

  jclass clazz = env->GetObjectClass(jdriver);

  jfieldID sched = env->GetFieldID(clazz, "sched", "Lmesos/Scheduler;");
  jobject jsched = env->GetObjectField(jdriver, sched);

  clazz = env->GetObjectClass(jsched);

  // ExecutorInfo execInfo = sched.getExecutorInfo(driver);
  jmethodID getExecutorInfo =
    env->GetMethodID(clazz, "getExecutorInfo", "(Lmesos/SchedulerDriver;)Lmesos/ExecutorInfo;");

  env->ExceptionClear();

  jobject jexecInfo = env->CallObjectMethod(jsched, getExecutorInfo, jdriver);

  if (!env->ExceptionOccurred()) {
    execInfo = construct<ExecutorInfo>(env, jexecInfo);
    jvm->DetachCurrentThread();
  } else {
    env->ExceptionDescribe();
    env->ExceptionClear();
    jvm->DetachCurrentThread();
    driver->stop();
    this->error(driver, -1, "Java exception caught");
  }

  return execInfo;
}


void JNIScheduler::registered(SchedulerDriver* driver, FrameworkID frameworkId)
{
  jvm->AttachCurrentThread((void **) &env, NULL);

  jclass clazz = env->GetObjectClass(jdriver);

  jfieldID sched = env->GetFieldID(clazz, "sched", "Lmesos/Scheduler;");
  jobject jsched = env->GetObjectField(jdriver, sched);

  clazz = env->GetObjectClass(jsched);

  // sched.registered(driver, frameworkId);
  jmethodID registered = env->GetMethodID(clazz, "registered",
    "(Lmesos/SchedulerDriver;Lmesos/FrameworkID;)V");

  jobject jframeworkId = convert<FrameworkID>(env, frameworkId);

  env->ExceptionClear();

  env->CallVoidMethod(jsched, registered, jdriver, jframeworkId);

  if (!env->ExceptionOccurred()) {
    jvm->DetachCurrentThread();
  } else {
    env->ExceptionDescribe();
    env->ExceptionClear();
    jvm->DetachCurrentThread();
    driver->stop();
    this->error(driver, -1, "Java exception caught");
  }
}


void JNIScheduler::resourceOffer(SchedulerDriver* driver, OfferID offerId,
                                 const vector<SlaveOffer>& offers)
{
  jvm->AttachCurrentThread((void **) &env, NULL);

  jclass clazz = env->GetObjectClass(jdriver);

  jfieldID sched = env->GetFieldID(clazz, "sched", "Lmesos/Scheduler;");
  jobject jsched = env->GetObjectField(jdriver, sched);

  clazz = env->GetObjectClass(jsched);

  // sched.resourceOffer(driver, offerId, offers);
  jmethodID resourceOffer = env->GetMethodID(clazz, "resourceOffer",
    "(Lmesos/SchedulerDriver;Lmesos/OfferID;Ljava/util/List;)V");

  jobject jofferId = convert<OfferID>(env, offerId);

  // List offers = new ArrayList();
  clazz = env->FindClass("java/util/ArrayList");

  jmethodID _init_ = env->GetMethodID(clazz, "<init>", "()V");
  jobject joffers = env->NewObject(clazz, _init_);

  jmethodID add = env->GetMethodID(clazz, "add", "(Ljava/lang/Object;)Z");
    
  // Loop through C++ vector and add each offer to the Java vector.
  foreach (const SlaveOffer &offer, offers) {
    jobject joffer = convert<SlaveOffer>(env, offer);
    env->CallBooleanMethod(joffers, add, joffer);
  }

  env->ExceptionClear();

  env->CallVoidMethod(jsched, resourceOffer, jdriver, jofferId, joffers);

  if (!env->ExceptionOccurred()) {
    jvm->DetachCurrentThread();
  } else {
    env->ExceptionDescribe();
    env->ExceptionClear();
    jvm->DetachCurrentThread();
    driver->stop();
    this->error(driver, -1, "Java exception caught");
  }
}


void JNIScheduler::offerRescinded(SchedulerDriver* driver, OfferID offerId)
{
  jvm->AttachCurrentThread((void **) &env, NULL);

  jclass clazz = env->GetObjectClass(jdriver);

  jfieldID sched = env->GetFieldID(clazz, "sched", "Lmesos/Scheduler;");
  jobject jsched = env->GetObjectField(jdriver, sched);

  clazz = env->GetObjectClass(jsched);

  // sched.offerRescinded(driver, offerId);
  jmethodID offerRescinded = env->GetMethodID(clazz, "offerRescinded",
    "(Lmesos/SchedulerDriver;Lmesos/OfferID;)V");

  jobject jofferId = convert<OfferID>(env, offerId);

  env->ExceptionClear();

  env->CallVoidMethod(jsched, offerRescinded, jdriver, jofferId);

  if (!env->ExceptionOccurred()) {
    jvm->DetachCurrentThread();
  } else {
    env->ExceptionDescribe();
    env->ExceptionClear();
    jvm->DetachCurrentThread();
    driver->stop();
    this->error(driver, -1, "Java exception caught");
  }
}


void JNIScheduler::statusUpdate(SchedulerDriver* driver, const TaskStatus& status)
{
  jvm->AttachCurrentThread((void **) &env, NULL);

  jclass clazz = env->GetObjectClass(jdriver);

  jfieldID sched = env->GetFieldID(clazz, "sched", "Lmesos/Scheduler;");
  jobject jsched = env->GetObjectField(jdriver, sched);

  clazz = env->GetObjectClass(jsched);

  // sched.statusUpdate(driver, status);
  jmethodID statusUpdate = env->GetMethodID(clazz, "statusUpdate",
    "(Lmesos/SchedulerDriver;Lmesos/TaskStatus;)V");

  jobject jstatus = convert<TaskStatus>(env, status);

  env->ExceptionClear();

  env->CallVoidMethod(jsched, statusUpdate, jdriver, jstatus);

  if (!env->ExceptionOccurred()) {
    jvm->DetachCurrentThread();
  } else {
    env->ExceptionDescribe();
    env->ExceptionClear();
    jvm->DetachCurrentThread();
    driver->stop();
    this->error(driver, -1, "Java exception caught");
  }
}


void JNIScheduler::frameworkMessage(SchedulerDriver* driver, const FrameworkMessage& message)
{
  jvm->AttachCurrentThread((void **) &env, NULL);

  jclass clazz = env->GetObjectClass(jdriver);

  jfieldID sched = env->GetFieldID(clazz, "sched", "Lmesos/Scheduler;");
  jobject jsched = env->GetObjectField(jdriver, sched);

  clazz = env->GetObjectClass(jsched);

  // sched.frameworkMessage(driver, message);
  jmethodID frameworkMessage = env->GetMethodID(clazz, "frameworkMessage",
    "(Lmesos/SchedulerDriver;Lmesos/FrameworkMessage;)V");

  jobject jmessage = convert<FrameworkMessage>(env, message);

  env->ExceptionClear();

  env->CallVoidMethod(jsched, frameworkMessage, jdriver, jmessage);

  if (!env->ExceptionOccurred()) {
    jvm->DetachCurrentThread();
  } else {
    env->ExceptionDescribe();
    env->ExceptionClear();
    jvm->DetachCurrentThread();
    driver->stop();
    this->error(driver, -1, "Java exception caught");
  }
}


void JNIScheduler::slaveLost(SchedulerDriver* driver, SlaveID slaveId)
{
  jvm->AttachCurrentThread((void **) &env, NULL);

  jclass clazz = env->GetObjectClass(jdriver);

  jfieldID sched = env->GetFieldID(clazz, "sched", "Lmesos/Scheduler;");
  jobject jsched = env->GetObjectField(jdriver, sched);

  clazz = env->GetObjectClass(jsched);

  // sched.slaveLost(driver, slaveId);
  jmethodID slaveLost = env->GetMethodID(clazz, "slaveLost",
    "(Lmesos/SchedulerDriver;Lmesos/SlaveID;)V");

  jobject jslaveId = convert<SlaveID>(env, slaveId);

  env->ExceptionClear();

  env->CallVoidMethod(jsched, slaveLost, jdriver, jslaveId);

  if (!env->ExceptionOccurred()) {
    jvm->DetachCurrentThread();
  } else {
    env->ExceptionDescribe();
    env->ExceptionClear();
    jvm->DetachCurrentThread();
    driver->stop();
    this->error(driver, -1, "Java exception caught");
  }
}


void JNIScheduler::error(SchedulerDriver* driver, int code, const string& message)
{
  jvm->AttachCurrentThread((void **) &env, NULL);

  jclass clazz = env->GetObjectClass(jdriver);

  jfieldID sched = env->GetFieldID(clazz, "sched", "Lmesos/Scheduler;");
  jobject jsched = env->GetObjectField(jdriver, sched);

  clazz = env->GetObjectClass(jsched);

  // sched.error(driver, code, message);
  jmethodID error = env->GetMethodID(clazz, "error",
    "(Lmesos/SchedulerDriver;ILjava/lang/String;)V");

  jint jcode = code;
  jobject jmessage = convert<string>(env, message);

  env->ExceptionClear();

  env->CallVoidMethod(jsched, error, jdriver, jcode, jmessage);

  if (!env->ExceptionOccurred()) {
    jvm->DetachCurrentThread();
  } else {
    env->ExceptionDescribe();
    env->ExceptionClear();
    jvm->DetachCurrentThread();
    driver->stop();
    // Don't call error recursively here!
  }
}


#ifdef __cplusplus
extern "C" {
#endif

/*
 * Class:     mesos_MesosSchedulerDriver
 * Method:    initialize
 * Signature: ()V
 */
JNIEXPORT void JNICALL Java_mesos_MesosSchedulerDriver_initialize
  (JNIEnv *env, jobject thiz)
{
  jclass clazz = env->GetObjectClass(thiz);

  // Create a global reference to the MesosSchedulerDriver instance.
  jobject jdriver = env->NewWeakGlobalRef(thiz);

  // Create the C++ scheduler and initialize the __sched variable.
  JNIScheduler *sched = new JNIScheduler(env, jdriver);

  jfieldID __sched = env->GetFieldID(clazz, "__sched", "J");
  env->SetLongField(thiz, __sched, (jlong) sched);

  // Get out the url passed into the constructor.
  jfieldID url = env->GetFieldID(clazz, "url", "Ljava/lang/String;");
  jobject jstr = env->GetObjectField(thiz, url);
  
  // Create the C++ driver and initialize the __driver variable.
  MesosSchedulerDriver *driver =
    new MesosSchedulerDriver(sched, construct<string>(env, jstr));

  jfieldID __driver = env->GetFieldID(clazz, "__driver", "J");
  env->SetLongField(thiz, __driver, (jlong) driver);
}


/*
 * Class:     mesos_MesosSchedulerDriver
 * Method:    finalize
 * Signature: ()V
 */
JNIEXPORT void JNICALL Java_mesos_MesosSchedulerDriver_finalize
  (JNIEnv *env, jobject thiz)
{
  jclass clazz = env->GetObjectClass(thiz);

  jfieldID __driver = env->GetFieldID(clazz, "__driver", "J");
  MesosSchedulerDriver *driver = (MesosSchedulerDriver *)
    env->GetLongField(thiz, __driver);

  // Call stop just in case.
  driver->stop();
  driver->join();

  delete driver;

  jfieldID __sched = env->GetFieldID(clazz, "__sched", "J");
  JNIScheduler *sched = (JNIScheduler *)
    env->GetLongField(thiz, __sched);

  env->DeleteWeakGlobalRef(sched->jdriver);

  delete sched;
}


/*
 * Class:     mesos_MesosSchedulerDriver
 * Method:    start
 * Signature: ()I
 */
JNIEXPORT jint JNICALL Java_mesos_MesosSchedulerDriver_start
  (JNIEnv *env, jobject thiz)
{
  jclass clazz = env->GetObjectClass(thiz);

  jfieldID __driver = env->GetFieldID(clazz, "__driver", "J");
  MesosSchedulerDriver *driver = (MesosSchedulerDriver *)
    env->GetLongField(thiz, __driver);

  return driver->start();
}


/*
 * Class:     mesos_MesosSchedulerDriver
 * Method:    stop
 * Signature: ()I
 */
JNIEXPORT jint JNICALL Java_mesos_MesosSchedulerDriver_stop
  (JNIEnv *env, jobject thiz)
{
  jclass clazz = env->GetObjectClass(thiz);

  jfieldID __driver = env->GetFieldID(clazz, "__driver", "J");
  MesosSchedulerDriver *driver = (MesosSchedulerDriver *)
    env->GetLongField(thiz, __driver);

  return driver->stop();
}


/*
 * Class:     mesos_MesosSchedulerDriver
 * Method:    join
 * Signature: ()I
 */
JNIEXPORT jint JNICALL Java_mesos_MesosSchedulerDriver_join
  (JNIEnv *env, jobject thiz)
{
  jclass clazz = env->GetObjectClass(thiz);

  jfieldID __driver = env->GetFieldID(clazz, "__driver", "J");
  MesosSchedulerDriver *driver = (MesosSchedulerDriver *)
    env->GetLongField(thiz, __driver);

  return driver->join();
}


/*
 * Class:     mesos_MesosSchedulerDriver
 * Method:    sendFrameworkMessage
 * Signature: (Lmesos/FrameworkMessage;)I
 */
JNIEXPORT jint JNICALL Java_mesos_MesosSchedulerDriver_sendFrameworkMessage
  (JNIEnv *env, jobject thiz, jobject jmessage)
{
  // Construct a C++ FrameworkMessage from the Java FrameworkMessage.
  FrameworkMessage message = construct<FrameworkMessage>(env, jmessage);

  // Now invoke the underlying driver.
  jclass clazz = env->GetObjectClass(thiz);

  jfieldID __driver = env->GetFieldID(clazz, "__driver", "J");
  MesosSchedulerDriver *driver = (MesosSchedulerDriver *)
    env->GetLongField(thiz, __driver);

  return driver->sendFrameworkMessage(message);
}


/*
 * Class:     mesos_MesosSchedulerDriver
 * Method:    killTask
 * Signature: (Lmesos/TaskID;)I
 */
JNIEXPORT jint JNICALL Java_mesos_MesosSchedulerDriver_killTask
  (JNIEnv *env, jobject thiz, jobject jtaskId)
{
  // Construct a C++ TaskID from the Java TaskId.
  TaskID taskId = construct<TaskID>(env, jtaskId);

  // Now invoke the underlying driver.
  jclass clazz = env->GetObjectClass(thiz);

  jfieldID __driver = env->GetFieldID(clazz, "__driver", "J");
  MesosSchedulerDriver *driver = (MesosSchedulerDriver *)
    env->GetLongField(thiz, __driver);

  return driver->killTask(taskId);
}


/*
 * Class:     mesos_MesosSchedulerDriver
 * Method:    replyToOffer
 * Signature: (Lmesos/OfferID;Ljava/util/Collection;Ljava/util/Map;)I
 */
JNIEXPORT jint JNICALL Java_mesos_MesosSchedulerDriver_replyToOffer
  (JNIEnv *env, jobject thiz, jobject jofferId, jobject jtasks, jobject jparams)
{
  // Construct a C++ OfferID from the Java OfferID.
  OfferID offerId = construct<OfferID>(env, jofferId);

  // Construct a C++ TaskDescription from each Java TaskDescription.
  vector<TaskDescription> tasks;

  jclass clazz = env->GetObjectClass(jtasks);

  // Iterator iterator = tasks.iterator();
  jmethodID iterator = env->GetMethodID(clazz, "iterator", "()Ljava/util/Iterator;");
  jobject jiterator = env->CallObjectMethod(jtasks, iterator);

  clazz = env->GetObjectClass(jiterator);

  // while (iterator.hasNext()) {
  jmethodID hasNext = env->GetMethodID(clazz, "hasNext", "()Z");

  jmethodID next = env->GetMethodID(clazz, "next", "()Ljava/lang/Object;");

  while (env->CallBooleanMethod(jiterator, hasNext)) {
    // Object task = iterator.next();
    jobject jtask = env->CallObjectMethod(jiterator, next);
    TaskDescription task = construct<TaskDescription>(env, jtask);
    tasks.push_back(task);
  }

  // Construct a C++ map<string, string> from the Java Map<String, String>.
  map<string, string> params = construct< map<string, string> >(env, jparams);

  // Now invoke the underlying driver.
  clazz = env->GetObjectClass(thiz);

  jfieldID __driver = env->GetFieldID(clazz, "__driver", "J");
  MesosSchedulerDriver *driver = (MesosSchedulerDriver *)
    env->GetLongField(thiz, __driver);

  return driver->replyToOffer(offerId, tasks, params);
}


/*
 * Class:     mesos_MesosSchedulerDriver
 * Method:    reviveOffers
 * Signature: ()I
 */
JNIEXPORT jint JNICALL Java_mesos_MesosSchedulerDriver_reviveOffers
  (JNIEnv *env, jobject thiz)
{
  jclass clazz = env->GetObjectClass(thiz);

  jfieldID __driver = env->GetFieldID(clazz, "__driver", "J");
  MesosSchedulerDriver *driver = (MesosSchedulerDriver *)
    env->GetLongField(thiz, __driver);

  return driver->reviveOffers();
}


/*
 * Class:     mesos_MesosSchedulerDriver
 * Method:    sendHints
 * Signature: (Ljava/util/Map;)I
 */
JNIEXPORT jint JNICALL Java_mesos_MesosSchedulerDriver_sendHints
  (JNIEnv *env, jobject thiz, jobject jhints)
{
  // Construct a C++ map<string, string> from the Java Map<String, String>.
  map<string, string> hints = construct< map<string, string> >(env, jhints);

  // Now invoke the underlying driver.
  jclass clazz = env->GetObjectClass(thiz);

  jfieldID __driver = env->GetFieldID(clazz, "__driver", "J");
  MesosSchedulerDriver *driver = (MesosSchedulerDriver *)
    env->GetLongField(thiz, __driver);

  return driver->sendHints(hints);
}

#ifdef __cplusplus
}
#endif
