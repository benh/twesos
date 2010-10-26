package mesos;


public class TaskID {
  public TaskID(int i) {
    this.i = i;
  }

  public boolean equals(Object that) {
    if (this == that)
      return true;

    if (!(that instanceof TaskID))
      return false;

    TaskID other = (TaskID) that;

    return i == other.i;
  }

  public int hashCode() {
    return i;
  }

  public String toString() {
    return Integer.toString(i);
  }

  private final int i;
}
