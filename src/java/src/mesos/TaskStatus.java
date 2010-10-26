package mesos;


public class TaskStatus {
  public TaskStatus(TaskID taskId, TaskState state, byte[] data) {
    this.taskId = taskId;
    this.state = state;
    this.data = data;
  }

  public TaskStatus(TaskID taskId, TaskState state) {
    this(taskId, state, new byte[0]);
  }

  @Override
  public boolean equals(Object that) {
    if (that == this)
      return true;

    if (!(that instanceof TaskStatus))
      return false;

    TaskStatus other = (TaskStatus) that;

    return taskId.equals(other.taskId) &&
      state.equals(other.state) &&
      data.equals(other.data);
  }

  @Override
  public int hashCode() {
    int hash = 1;
    hash = hash * 31 + (taskId != null ? taskId.hashCode() : 0);
    hash = hash * 31 + (state != null ? state.hashCode() : 0);
    hash = hash * 31 + (data != null ? data.hashCode() : 0);
    return hash;
  }

  @Override
  public String toString() {
    return "TaskStatus {\n" +
      "  .taskId = " + taskId + "\n" +
      "  .state = " + state + "\n" +
      "  .data = " + data + "\n" +
      "}";
  }

  public final TaskID taskId;
  public final TaskState state;
  public final byte[] data;
}
