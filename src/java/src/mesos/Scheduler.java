package mesos;

import java.util.List;


/**
 * Callback interface to be implemented by frameworks' schedulers.
 */
public interface Scheduler {
  public String getFrameworkName(SchedulerDriver d);
  public ExecutorInfo getExecutorInfo(SchedulerDriver d);
  public void registered(SchedulerDriver d, FrameworkID frameworkId);
  public void resourceOffer(SchedulerDriver d, OfferID offerId, List<SlaveOffer> offers);
  public void offerRescinded(SchedulerDriver d, OfferID offerId);
  public void statusUpdate(SchedulerDriver d, TaskStatus status);
  public void frameworkMessage(SchedulerDriver d, FrameworkMessage message);
  public void slaveLost(SchedulerDriver d, SlaveID slaveId);
  public void error(SchedulerDriver d, int code, String message);
}
