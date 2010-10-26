package mesos;

import java.util.Collection;
import java.util.Map;


public interface SchedulerDriver {
  // Lifecycle methods.
  public int start();
  public int stop();
  public int join();
  public int run();

  // Communication methods.
  public int sendFrameworkMessage(FrameworkMessage message);
  public int killTask(TaskID taskId);
  public int replyToOffer(OfferID offerId, Collection<TaskDescription> tasks, Map<String, String> params);
  public int replyToOffer(OfferID offerId, Collection<TaskDescription> tasks);
  public int reviveOffers();
  public int sendHints(Map<String, String> hints);
};
