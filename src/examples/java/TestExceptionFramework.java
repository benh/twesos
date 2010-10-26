import java.io.File;

import java.util.List;

import mesos.*;


public class TestExceptionFramework {
  static class MyScheduler implements Scheduler {
    @Override
    public String getFrameworkName(SchedulerDriver d) {
      throw new ArrayIndexOutOfBoundsException();
    }

    @Override
    public ExecutorInfo getExecutorInfo(SchedulerDriver d) {
      try {
        File file = new File("./test_executor");
        return new ExecutorInfo(file.getCanonicalPath());
      } catch (Throwable t) {
        throw new RuntimeException(t);
      }
    }

    @Override
    public void registered(SchedulerDriver d, FrameworkID frameworkId) {}

    @Override
    public void resourceOffer(SchedulerDriver d,
                              OfferID offerId,
                              List<SlaveOffer> offers) {}

    @Override
    public void offerRescinded(SchedulerDriver d, OfferID offerId) {}

    @Override
    public void statusUpdate(SchedulerDriver d, TaskStatus status) {}

    @Override
    public void frameworkMessage(SchedulerDriver d, FrameworkMessage message) {}

    @Override
    public void slaveLost(SchedulerDriver d, SlaveID slaveId) {}

    @Override
    public void error(SchedulerDriver d, int code, String message) {}
  }

  public static void main(String[] args) throws Exception {
    new MesosSchedulerDriver(new MyScheduler(), args[0]).run();
  }
}
