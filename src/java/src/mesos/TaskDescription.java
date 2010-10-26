package mesos;

import java.util.Collections;
import java.util.Map;


public class TaskDescription {
  public TaskDescription(TaskID taskId, SlaveID slaveId, String name, Map<String, String> params, byte[] data) {
    this.taskId = taskId;
    this.slaveId = slaveId;
    this.name = name;
    this.params = params;
    this.data = data;
  }

  public TaskDescription(TaskID taskId, SlaveID slaveId, String name, byte[] data) {
      this(taskId, slaveId, name, Collections.<String, String>emptyMap(), data);
  }

  public TaskDescription(TaskID taskId, SlaveID slaveId, String name, Map<String, String> params) {
      this(taskId, slaveId, name, params, new byte[0]);
  }

  @Override
  public boolean equals(Object that) {
    if (that == this)
      return true;

    if (!(that instanceof TaskDescription))
      return false;

    TaskDescription other = (TaskDescription) that;

    return taskId.equals(other.taskId) &&
      slaveId.equals(other.slaveId) &&
      name.equals(other.name) &&
      params.equals(other.params) &&
      data.equals(other.data);
  }

  @Override
  public int hashCode() {
    int hash = 1;
    hash = hash * 31 + (taskId != null ? taskId.hashCode() : 0);
    hash = hash * 31 + (slaveId != null ? slaveId.hashCode() : 0);
    hash = hash * 31 + (name != null ? name.hashCode() : 0);
    hash = hash * 31 + (params != null ? params.hashCode() : 0);
    hash = hash * 31 + (data != null ? data.hashCode() : 0);
    return hash;
  }

  @Override
  public String toString() {
    return "TaskDescription {\n" +
      "  .taskId = " + taskId + "\n" +
      "  .slaveId = " + slaveId + "\n" +
      "  .name = " + name + "\n" +
      "  .params = " + params + "\n" +
      "  .data = " + data + "\n" +
      "}";
  }

  public final TaskID taskId;
  public final SlaveID slaveId;
  public final String name;
  public final Map<String, String> params;
  public final byte[] data;
}
