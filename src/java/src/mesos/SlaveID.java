package mesos;


public class SlaveID {
  public SlaveID(String s) {
    this.s = s;
  }

  public boolean equals(Object that) {
    if (this == that)
      return true;

    if (!(that instanceof SlaveID))
      return false;

    SlaveID other = (SlaveID) that;

    return s.equals(other.s);
  }

  public int hashCode() {
    return s.hashCode();
  }

  public String toString() {
    return s;
  }

  private final String s;
}
