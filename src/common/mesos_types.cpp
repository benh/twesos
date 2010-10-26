#include <iostream>

#include <boost/unordered_map.hpp>

#include "mesos_types.hpp"

using std::istream;
using std::ostream;
using std::size_t;


namespace mesos {

ostream& operator << (ostream& out, const FrameworkID& id)
{
  out << id.s;
}


istream& operator >> (istream& in, FrameworkID& id)
{
  in >> id.s;
}


size_t hash_value(const FrameworkID& id)
{
  return boost::hash_value(id.s);
}


ostream& operator << (ostream& out, const SlaveID& id)
{
  out << id.s;
}


istream& operator >> (istream& in, SlaveID& id)
{
  in >> id.s;
}


size_t hash_value(const SlaveID& id)
{
  return boost::hash_value(id.s);
}


ostream& operator << (ostream& out, const OfferID& id)
{
  out << id.s;
}


istream& operator >> (istream& in, OfferID& id)
{
  in >> id.s;
}


size_t hash_value(const OfferID& id)
{
  return boost::hash_value(id.s);
}

} /* namespace mesos { */
