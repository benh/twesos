package mesos;


/**
 * Callback interface to be implemented by frameworks' executors.
 */
public interface Executor {
  public void init(ExecutorDriver d, ExecutorArgs args);
  public void launchTask(ExecutorDriver d, TaskDescription task);
  public void killTask(ExecutorDriver d, TaskID taskId);
  public void frameworkMessage(ExecutorDriver d, FrameworkMessage message);
  public void shutdown(ExecutorDriver d);
  public void error(ExecutorDriver d, int code, String message);
}
