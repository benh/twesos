package mesos;

import java.util.Map;


public class SlaveOffer {
  public SlaveOffer(SlaveID slaveId, String host, Map<String, String> params) {
    this.slaveId = slaveId;
    this.host = host;
    this.params = params;
  }

  @Override
  public boolean equals(Object that) {
    if (that == this)
      return true;

    if (!(that instanceof SlaveOffer))
      return false;

    SlaveOffer other = (SlaveOffer) that;

    return slaveId.equals(other.slaveId) &&
      host.equals(other.host) &&
      params.equals(other.params);
  }

  @Override
  public int hashCode() {
    int hash = 1;
    hash = hash * 31 + (slaveId != null ? slaveId.hashCode() : 0);
    hash = hash * 31 + (host != null ? host.hashCode() : 0);
    hash = hash * 31 + (params != null ? params.hashCode() : 0);
    return hash;
  }

  @Override
  public String toString() {
    return "SlaveOffer {\n" +
      "  .slaveId = " + slaveId + "\n" +
      "  .host = " + host + "\n" +
      "  .params = " + params + "\n" +
      "}";
  }

  public final SlaveID slaveId;
  public final String host;
  public final Map<String, String> params;
}
