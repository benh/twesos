package mesos;


public class FrameworkID {
  public static final FrameworkID EMPTY = new FrameworkID("");

  public FrameworkID(String s) {
    this.s = s;
  }

  public boolean equals(Object that) {
    if (this == that)
      return true;

    if (!(that instanceof FrameworkID))
      return false;

    FrameworkID other = (FrameworkID) that;

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