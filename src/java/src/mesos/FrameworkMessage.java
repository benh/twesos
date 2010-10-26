package mesos;


public class FrameworkMessage {
  public FrameworkMessage(SlaveID slaveId, TaskID taskId, byte[] data) {
    this.slaveId = slaveId;
    this.taskId = taskId;
    this.data = data;
  }

  @Override
  public boolean equals(Object that) {
    if (that == this)
      return true;

    if (!(that instanceof FrameworkMessage))
      return false;

    FrameworkMessage other = (FrameworkMessage) that;

    return slaveId.equals(other.slaveId) &&
      taskId.equals(other.taskId) &&
      data.equals(other.data);
  }

  @Override
  public int hashCode() {
    int hash = 1;
    hash = hash * 31 + (slaveId != null ? slaveId.hashCode() : 0);
    hash = hash * 31 + (taskId != null ? taskId.hashCode() : 0);
    hash = hash * 31 + (data != null ? data.hashCode() : 0);
    return hash;
  }

  @Override
  public String toString() {
    return "FrameworkMessage {\n" +
      "  .slaveId = " + slaveId + "\n" +
      "  .taskId = " + taskId + "\n" +
      "  .data = " + data + "\n" +
      "}";
  }

  public final SlaveID slaveId;
  public final TaskID taskId;
  public final byte[] data;
}
