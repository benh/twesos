import java.io.File;

import mesos.*;


public class TestExecutor implements Executor {
  @Override
  public void init(ExecutorDriver d, ExecutorArgs args) {}

  @Override
  public void launchTask(final ExecutorDriver d, final TaskDescription task) {
    new Thread() { public void run() {
      try {
        System.out.println("Running task " + task.taskId);
        Thread.sleep(1000);
        d.sendStatusUpdate(new TaskStatus(task.taskId,
                                          TaskState.TASK_FINISHED,
                                          new byte[0]));
      } catch (Exception e) {
        e.printStackTrace();
      }
    }}.start();
  }

  @Override
  public void killTask(ExecutorDriver d, TaskID taskId) {}

  @Override
  public void frameworkMessage(ExecutorDriver d, FrameworkMessage message) {}

  @Override
  public void shutdown(ExecutorDriver d) {}

  @Override
  public void error(ExecutorDriver d, int code, String message) {}

  public static void main(String[] args) throws Exception {
    new MesosExecutorDriver(new TestExecutor()).run();
  }
}
