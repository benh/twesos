package mesos;


/**
 * Arguments passed to executors on initialization.
 */
public class ExecutorArgs {
  public ExecutorArgs(SlaveID slaveId, String host, FrameworkID frameworkId, String frameworkName, byte[] data) {
    this.slaveId = slaveId;
    this.host = host;
    this.frameworkId = frameworkId;
    this.frameworkName = frameworkName;
    this.data = data;
  }

  @Override
  public boolean equals(Object that) {
    if (that == this)
      return true;

    if (!(that instanceof ExecutorArgs))
      return false;

    ExecutorArgs other = (ExecutorArgs) that;

    return slaveId.equals(other.slaveId) &&
      host.equals(other.host) &&
      frameworkId.equals(other.frameworkId) &&
      frameworkName.equals(other.frameworkName) &&
      data.equals(other.data);
  }

  @Override
  public int hashCode() {
    int hash = 1;
    hash = hash * 31 + (slaveId != null ? slaveId.hashCode() : 0);
    hash = hash * 31 + (host != null ? host.hashCode() : 0);
    hash = hash * 31 + (frameworkId != null ? frameworkId.hashCode() : 0);
    hash = hash * 31 + (frameworkName != null ? frameworkName.hashCode() : 0);
    hash = hash * 31 + (data != null ? data.hashCode() : 0);
    return hash;
  }

  @Override
  public String toString() {
    return "ExecutorArgs {\n" +
      "  .slaveId = " + slaveId + "\n" +
      "  .host = " + host + "\n" +
      "  .frameworkid = " + frameworkId + "\n" +
      "  .frameworkName = " + frameworkName + "\n" +
      "  .data = " + data + "\n" +
      "}";
  }

  public final SlaveID slaveId;
  public final String host;
  public final FrameworkID frameworkId;
  public final String frameworkName;
  public final byte[] data;
};
