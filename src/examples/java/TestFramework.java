import java.io.File;
import java.io.IOException;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

import mesos.*;


public class TestFramework {
  static class MyScheduler implements Scheduler {
    int launchedTasks = 0;
    int finishedTasks = 0;
    int totalTasks = 5;

    public MyScheduler() {}

    public MyScheduler(int numTasks) {
      totalTasks = numTasks;
    }

    @Override
    public String getFrameworkName(SchedulerDriver d) {
      return "Java test framework";
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
    public void registered(SchedulerDriver d, FrameworkID frameworkId) {
      System.out.println("Registered! ID = " + frameworkId);
    }

    @Override
    public void resourceOffer(SchedulerDriver d,
                              OfferID offerId ,
                              List<SlaveOffer> offers) {
      System.out.println("Got offer " + offerId);
      List<TaskDescription> tasks = new ArrayList<TaskDescription>();
      for (SlaveOffer offer: offers) {
        System.out.println("Params for slave " + offer.slaveId + ":");
        for (Map.Entry<String, String> entry: offer.params.entrySet()) {
          System.out.println(entry.getKey() + " = " + entry.getValue());
        }
        if (launchedTasks < totalTasks) {
          TaskID taskId = new TaskID(launchedTasks++);
          Map<String, String> taskParams = new HashMap<String, String>();
          taskParams.put("cpus", "1");
          taskParams.put("mem", "128");
          System.out.println("Launching task " + taskId);
          tasks.add(new TaskDescription(taskId,
                                        offer.slaveId,
                                        "task " + taskId,
                                        taskParams));
        }
      }
      Map<String, String> params = new HashMap<String, String>();
      params.put("timeout", "1");
      d.replyToOffer(offerId, tasks, params);
    }

    @Override
    public void offerRescinded(SchedulerDriver d, OfferID offerId) {}

    @Override
    public void statusUpdate(SchedulerDriver d, TaskStatus status) {
      System.out.println("Status update: task " + status.taskId +
                         " is in state " + status.state);
      if (status.state == TaskState.TASK_FINISHED) {
        finishedTasks++;
        System.out.println("Finished tasks: " + finishedTasks);
        if (finishedTasks == totalTasks)
          d.stop();
      }
    }

    @Override
    public void frameworkMessage(SchedulerDriver d, FrameworkMessage message) {}

    @Override
    public void slaveLost(SchedulerDriver d, SlaveID slaveId) {}

    @Override
    public void error(SchedulerDriver d, int code, String message) {
      System.out.println("Error: " + message);
    }
  }

  public static void main(String[] args) throws Exception {
    if (args.length < 1 || args.length > 2) {
      System.out.println("Invalid use: please specify a master");
    } else if (args.length == 1) {
      new MesosSchedulerDriver(new MyScheduler(),args[0]).run();
    } else {
      new MesosSchedulerDriver(new MyScheduler(Integer.parseInt(args[1])), args[0]).run();
    }
  }
}
