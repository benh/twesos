package mesos;

import java.util.Map;
import java.util.Collections;


public class ExecutorInfo {
  public ExecutorInfo(String uri, byte[] data, Map<String, String> params) {
    this.uri = uri;
    this.data = data;
    this.params = params;
  }

  public ExecutorInfo(String uri, Map<String, String> params) {
    this(uri, new byte[0], params);
  }

  public ExecutorInfo(String uri, byte[] data) {
    this(uri, data, Collections.<String, String>emptyMap());
  }

  public ExecutorInfo(String uri) {
    this(uri, new byte[0], Collections.<String, String>emptyMap());
  }

  @Override
  public boolean equals(Object that) {
    if (that == this)
      return true;

    if (!(that instanceof ExecutorInfo))
      return false;

    ExecutorInfo other = (ExecutorInfo) that;

    return uri.equals(other.uri) &&
      params.equals(other.params) &&
      data.equals(other.data);
  }
    
  @Override
  public int hashCode() {
    int hash = 1;
    hash = hash * 31 + (uri != null ? uri.hashCode() : 0);
    hash = hash * 31 + (params != null ? params.hashCode() : 0);
    hash = hash * 31 + (data != null ? data.hashCode() : 0);
    return hash;
  }

  @Override
  public String toString() {
    return "ExecutorInfo {\n" +
      "  .uri = " + uri + "\n" +
      "  .params = " + params + "\n" +
      "  .data = " + data + "\n" +
      "}";
  }

  public final String uri;
  public final Map<String, String> params;
  public final byte[] data;
};
