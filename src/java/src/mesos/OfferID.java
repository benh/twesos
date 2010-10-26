package mesos;


public class OfferID {
  public OfferID(String s) {
    this.s = s;
  }

  public boolean equals(Object that) {
    if (this == that)
      return true;

    if (!(that instanceof OfferID))
      return false;

    OfferID other = (OfferID) that;

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
