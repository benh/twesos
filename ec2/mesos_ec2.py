#!/usr/bin/env python
# -*- coding: utf-8 -*-

from __future__ import with_statement

import boto
import logging
import os
import shutil
import subprocess
import sys
import tempfile
import time
from optparse import OptionParser
from sys import stderr
from boto.ec2.blockdevicemapping import BlockDeviceMapping, EBSBlockDeviceType


# Configure and parse our command-line arguments
def parse_args():
  parser = OptionParser(usage="mesos-ec2 [options] <action> <cluster_name>"
      + "\n\n<action> can be: launch, destroy, login, stop, start, get-main",
      add_help_option=False)
  parser.add_option("-h", "--help", action="help",
                    help="Show this help message and exit")
  parser.add_option("-s", "--subordinates", type="int", default=1,
      help="Number of subordinates to launch (default: 1)")
  parser.add_option("-k", "--key-pair",
      help="Key pair to use on instances")
  parser.add_option("-i", "--identity-file", 
      help="SSH private key file to use for logging into instances")
  parser.add_option("-t", "--instance-type", default="m1.large",
      help="Type of instance to launch (default: m1.large). " +
           "WARNING: must be 64 bit, thus small instances won't work")
  parser.add_option("-m", "--main-instance-type", default="",
      help="Main instance type (leave empty for same as instance-type)")
  parser.add_option("-z", "--zone", default="us-east-1b",
      help="Availability zone to launch instances in")
  parser.add_option("-a", "--ami", default="ami-f8806a91",
      help="Amazon Machine Image ID to use")
  parser.add_option("-o", "--os", default="lucid64",
      help="OS on the Amazon Machine Image (lucid64 for now)")
  parser.add_option("-d", "--download", metavar="SOURCE", default="none",
      help="Where to download latest code from: set to 'git' to check out " +
           "from git, or 'none' to use the Mesos on the AMI (default)")
  parser.add_option("-b", "--branch", default="main",
      help="If using git, which branch to check out. Default is 'main'")
  parser.add_option("-D", metavar="[ADDRESS:]PORT", dest="proxy_port", 
      help="Use SSH dynamic port forwarding to create a SOCKS proxy at " +
            "the given local address (for use with login)")
  parser.add_option("--resume", action="store_true", default=False,
      help="Resume installation on a previously launched cluster " +
           "(for debugging)")
  parser.add_option("-f", "--ft", metavar="NUM_MASTERS", default="1", 
      help="Number of mains to run. Default is 1. " + 
           "Greater values cause Mesos to run in FT mode with ZooKeeper.")
  parser.add_option("--ebs-vol-size", metavar="SIZE", type="int", default=0,
      help="Attach a new EBS volume of size SIZE (in GB) to each node as " +
           "/vol. The volumes will be deleted when the instances terminate. " +
           "Only possible on EBS-backed AMIs.")
  (opts, args) = parser.parse_args()
  opts.ft = int(opts.ft)
  if len(args) != 2:
    parser.print_help()
    sys.exit(1)
  (action, cluster_name) = args
  if opts.identity_file == None and action in ['launch', 'login']:
    print >> stderr, ("ERROR: The -i or --identity-file argument is " +
                      "required for " + action)
    sys.exit(1)
  if os.getenv('AWS_ACCESS_KEY_ID') == None:
    print >> stderr, ("ERROR: The environment variable AWS_ACCESS_KEY_ID " +
                      "must be set")
    sys.exit(1)
  if os.getenv('AWS_SECRET_ACCESS_KEY') == None:
    print >> stderr, ("ERROR: The environment variable AWS_SECRET_ACCESS_KEY " +
                      "must be set")
    sys.exit(1)
  return (opts, action, cluster_name)


# Get the EC2 security group of the given name, creating it if it doesn't exist
def get_or_make_group(conn, name):
  groups = conn.get_all_security_groups()
  group = [g for g in groups if g.name == name]
  if len(group) > 0:
    return group[0]
  else:
    print "Creating security group " + name
    return conn.create_security_group(name, "Mesos EC2 group")


# Wait for a set of launched instances to exit the "pending" state
# (i.e. either to start running or to fail and be terminated)
def wait_for_instances(conn, reservation):
  instance_ids = [i.id for i in reservation.instances]
  while True:
    reservations = conn.get_all_instances(instance_ids)
    some_pending = False
    for res in reservations:
      if len([i for i in res.instances if i.state == 'pending']) > 0:
        some_pending = True
        break
    if some_pending:
      time.sleep(5)
    else:
      for i in reservation.instances:
        i.update()
      return


# Check whether a given EC2 instance object is in a state we consider active,
# i.e. not terminating or terminated. We count both stopping and stopped as
# active since we can restart stopped clusters.
def is_active(instance):
  return (instance.state in ['pending', 'running', 'stopping', 'stopped'])


# Launch a cluster of the given name, by setting up its security groups,
# and then starting new instances in them.
# Returns a tuple of EC2 reservation objects for the main, subordinate
# and zookeeper instances (in that order).
# Fails if there already instances running in the cluster's groups.
def launch_cluster(conn, opts, cluster_name):
  print "Setting up security groups..."
  main_group = get_or_make_group(conn, cluster_name + "-main")
  subordinate_group = get_or_make_group(conn, cluster_name + "-subordinates")
  zoo_group = get_or_make_group(conn, cluster_name + "-zoo")
  if main_group.rules == []: # Group was just now created
    main_group.authorize(src_group=main_group)
    main_group.authorize(src_group=subordinate_group)
    main_group.authorize(src_group=zoo_group)
    main_group.authorize('tcp', 22, 22, '0.0.0.0/0')
    main_group.authorize('tcp', 8080, 8081, '0.0.0.0/0')
    main_group.authorize('tcp', 50030, 50030, '0.0.0.0/0')
    main_group.authorize('tcp', 50070, 50070, '0.0.0.0/0')
  if subordinate_group.rules == []: # Group was just now created
    subordinate_group.authorize(src_group=main_group)
    subordinate_group.authorize(src_group=subordinate_group)
    subordinate_group.authorize(src_group=zoo_group)
    subordinate_group.authorize('tcp', 22, 22, '0.0.0.0/0')
    subordinate_group.authorize('tcp', 8080, 8081, '0.0.0.0/0')
    subordinate_group.authorize('tcp', 50060, 50060, '0.0.0.0/0')
    subordinate_group.authorize('tcp', 50075, 50075, '0.0.0.0/0')
  if zoo_group.rules == []: # Group was just now created
    zoo_group.authorize(src_group=main_group)
    zoo_group.authorize(src_group=subordinate_group)
    zoo_group.authorize(src_group=zoo_group)
    zoo_group.authorize('tcp', 22, 22, '0.0.0.0/0')
    zoo_group.authorize('tcp', 2181, 2181, '0.0.0.0/0')
    zoo_group.authorize('tcp', 2888, 2888, '0.0.0.0/0')
    zoo_group.authorize('tcp', 3888, 3888, '0.0.0.0/0')

  # Check if instances are already running in our groups
  print "Checking for running cluster..."
  reservations = conn.get_all_instances()
  for res in reservations:
    group_names = [g.id for g in res.groups]
    if main_group.name in group_names or subordinate_group.name in group_names or zoo_group.name in group_names:
      active = [i for i in res.instances if is_active(i)]
      if len(active) > 0:
        print >> stderr, ("ERROR: There are already instances running in " +
            "group %s, %s or %s" % (main_group.name, subordinate_group.name, zoo_group.name))
        sys.exit(1)
  print "Launching instances..."
  try:
    image = conn.get_all_images(image_ids=[opts.ami])[0]
  except:
    print >> stderr, "Could not find AMI " + opts.ami
    sys.exit(1)

  # Create block device mapping so that we can add an EBS volume if asked to
  block_map = BlockDeviceMapping()
  if opts.ebs_vol_size > 0:
    device = EBSBlockDeviceType()
    device.size = opts.ebs_vol_size
    device.delete_on_termination = True
    block_map["/dev/sdv"] = device

  # Launch subordinates
  subordinate_res = image.run(key_name = opts.key_pair,
                        security_groups = [subordinate_group],
                        instance_type = opts.instance_type,
                        placement = opts.zone,
                        min_count = opts.subordinates,
                        max_count = opts.subordinates,
                        block_device_map = block_map)
  print "Launched subordinates, regid = " + subordinate_res.id

  # Launch mains
  main_type = opts.main_instance_type
  if main_type == "":
    main_type = opts.instance_type
  main_res = image.run(key_name = opts.key_pair,
                         security_groups = [main_group],
                         instance_type = main_type,
                         placement = opts.zone,
                         min_count = opts.ft,
                         max_count = opts.ft,
                         block_device_map = block_map)
  print "Launched main, regid = " + main_res.id

  # Launch ZooKeeper nodes if required
  if opts.ft > 1:
    zoo_res = image.run(key_name = opts.key_pair,
                        security_groups = [zoo_group],
                        instance_type = opts.instance_type,
                        placement = opts.zone,
                        min_count = 3,
                        max_count = 3,
                        block_device_map = block_map)
    print "Launched zoo, regid = " + zoo_res.id
  else:
    zoo_res = None

  # Return all the instances
  return (main_res, subordinate_res, zoo_res)


# Get the EC2 instances in an existing cluster if available.
# Returns a tuple of EC2 reservation objects for the main, subordinate
# and zookeeper instances (in that order).
def get_existing_cluster(conn, opts, cluster_name):
  print "Searching for existing cluster " + cluster_name + "..."
  reservations = conn.get_all_instances()
  main_res = None
  subordinate_res = None
  zoo_res = None
  for res in reservations:
    active = [i for i in res.instances if is_active(i)]
    if len(active) > 0:
      group_names = [g.id for g in res.groups]
      if group_names == [cluster_name + "-main"]:
        main_res = res
      elif group_names == [cluster_name + "-subordinates"]:
        subordinate_res = res
      elif group_names == [cluster_name + "-zoo"]:
        zoo_res = res
  if main_res != None and subordinate_res != None:
    print "Found main regid: " + main_res.id
    print "Found subordinate regid: " + subordinate_res.id
    if zoo_res != None:
      print "Found zoo regid: " + zoo_res.id
    return (main_res, subordinate_res, zoo_res)
  else:
    if main_res == None and subordinate_res != None:
      print "ERROR: Could not find main in group " + cluster_name + "-main"
    elif main_res != None and subordinate_res == None:
      print "ERROR: Could not find subordinates in group " + cluster_name + "-subordinates"
    else:
      print "ERROR: Could not find any existing cluster"
    sys.exit(1)


# Deploy configuration files and run setup scripts on a newly launched
# or started EC2 cluster.
def setup_cluster(conn, main_res, subordinate_res, zoo_res, opts, deploy_ssh_key):
  print "Deploying files to main..."
  deploy_files(conn, "deploy." + opts.os, opts, main_res, subordinate_res, zoo_res)
  main = main_res.instances[0].public_dns_name
  if deploy_ssh_key:
    print "Copying SSH key %s to main..." % opts.identity_file
    ssh(main, opts, 'mkdir -p /root/.ssh')
    scp(main, opts, opts.identity_file, '/root/.ssh/id_rsa')
  print "Running setup on main..."
  ssh(main, opts, "chmod u+x mesos-ec2/setup")
  ssh(main, opts, "mesos-ec2/setup %s %s %s" % (opts.os, opts.download, opts.branch))
  print "Done!"


# Wait for a whole cluster (mains, subordinates and ZooKeeper) to start up
def wait_for_cluster(conn, main_res, subordinate_res, zoo_res):
  print "Waiting for instances to start up..."
  time.sleep(5)
  wait_for_instances(conn, main_res)
  wait_for_instances(conn, subordinate_res)
  if zoo_res != None:
    wait_for_instances(conn, zoo_res)
  print "Waiting 40 more seconds..."
  time.sleep(40)


# Get number of local disks available for a given EC2 instance type.
def get_num_disks(instance_type):
  if instance_type in ["m1.xlarge", "c1.xlarge", "m2.xlarge", "cc1.4xlarge"]:
    return 4
  elif instance_type in ["m1.small", "c1.medium"]:
    return 1
  else:
    return 2


# Deploy the configuration file templates in a given local directory to
# a cluster, filling in any template parameters with information about the
# cluster (e.g. lists of mains and subordinates). Files are only deployed to
# the first main instance in the cluster, and we expect the setup
# script to be run on that instance to copy them to other nodes.
def deploy_files(conn, root_dir, opts, main_res, subordinate_res, zoo_res):
  active_main = main_res.instances[0].public_dns_name

  num_disks = get_num_disks(opts.instance_type)
  hdfs_data_dirs = "/mnt/hdfs/dfs/data"
  mapred_local_dirs = "/mnt/hadoop/mrlocal"
  if num_disks > 1:
    for i in range(2, num_disks + 1):
      hdfs_data_dirs += ",/mnt%d/hdfs/dfs/data" % i
      mapred_local_dirs += ",/mnt%d/hadoop/mrlocal" % i

  if zoo_res != None:
    zoo_list = '\n'.join([i.public_dns_name for i in zoo_res.instances])
    cluster_url = "zoo://" + ",".join(
        ["%s:2181/mesos" % i.public_dns_name for i in zoo_res.instances])
  else:
    zoo_list = "NONE"
    cluster_url = "1@%s:5050" % active_main

  template_vars = {
    "main_list": '\n'.join([i.public_dns_name for i in main_res.instances]),
    "active_main": active_main,
    "subordinate_list": '\n'.join([i.public_dns_name for i in subordinate_res.instances]),
    "zoo_list": zoo_list,
    "cluster_url": cluster_url,
    "hdfs_data_dirs": hdfs_data_dirs,
    "mapred_local_dirs": mapred_local_dirs
  }

  # Create a temp directory in which we will place all the files to be
  # deployed after we substitue template parameters in them
  tmp_dir = tempfile.mkdtemp()
  for path, dirs, files in os.walk(root_dir):
    dest_dir = os.path.join('/', path[len(root_dir):])
    local_dir = tmp_dir + dest_dir
    if not os.path.exists(local_dir):
      os.makedirs(local_dir)
    for filename in files:
      if filename[0] not in '#.~' and filename[-1] != '~':
        dest_file = os.path.join(dest_dir, filename)
        local_file = tmp_dir + dest_file
        with open(os.path.join(path, filename)) as src:
          with open(local_file, "w") as dest:
            text = src.read()
            for key in template_vars:
              text = text.replace("{{" + key + "}}", template_vars[key])
            dest.write(text)
            dest.close()
  # rsync the whole directory over to the main machine
  command = (("rsync -rv -e 'ssh -o StrictHostKeyChecking=no -i %s' " + 
      "'%s/' 'root@%s:/'") % (opts.identity_file, tmp_dir, active_main))
  subprocess.check_call(command, shell=True)
  # Remove the temp directory we created above
  shutil.rmtree(tmp_dir)


# Copy a file to a given host through scp, throwing an exception if scp fails
def scp(host, opts, local_file, dest_file):
  subprocess.check_call(
      "scp -q -o StrictHostKeyChecking=no -i %s '%s' 'root@%s:%s'" %
      (opts.identity_file, local_file, host, dest_file), shell=True)


# Run a command on a host through ssh, throwing an exception if ssh fails
def ssh(host, opts, command):
  subprocess.check_call(
      "ssh -t -o StrictHostKeyChecking=no -i %s root@%s '%s'" %
      (opts.identity_file, host, command), shell=True)


def main():
  (opts, action, cluster_name) = parse_args()
  conn = boto.connect_ec2()

  if action == "launch":
    if opts.resume:
      (main_res, subordinate_res, zoo_res) = get_existing_cluster(
          conn, opts, cluster_name)
    else:
      (main_res, subordinate_res, zoo_res) = launch_cluster(
          conn, opts, cluster_name)
      wait_for_cluster(conn, main_res, subordinate_res, zoo_res)
    setup_cluster(conn, main_res, subordinate_res, zoo_res, opts, True)

  elif action == "destroy":
    response = raw_input("Are you sure you want to destroy the cluster " +
        cluster_name + "?\nALL DATA ON ALL NODES WILL BE LOST!!\n" +
        "Destroy cluster " + cluster_name + " (y/N): ")
    if response == "y":
      (main_res, subordinate_res, zoo_res) = get_existing_cluster(
          conn, opts, cluster_name)
      print "Terminating main..."
      for inst in main_res.instances:
        inst.terminate()
      print "Terminating subordinates..."
      for inst in subordinate_res.instances:
        inst.terminate()
      if zoo_res != None:
        print "Terminating zoo..."
        for inst in zoo_res.instances:
          inst.terminate()

  elif action == "login":
    (main_res, subordinate_res, zoo_res) = get_existing_cluster(
        conn, opts, cluster_name)
    main = main_res.instances[0].public_dns_name
    print "Logging into main " + main + "..."
    proxy_opt = ""
    if opts.proxy_port != None:
      proxy_opt = "-D " + opts.proxy_port
    subprocess.check_call("ssh -o StrictHostKeyChecking=no -i %s %s root@%s" %
        (opts.identity_file, proxy_opt, main), shell=True)

  elif action == "get-main":
    (main_res, subordinate_res, zoo_res) = get_existing_cluster(conn, opts, cluster_name)
    print main_res.instances[0].public_dns_name

  elif action == "stop":
    response = raw_input("Are you sure you want to stop the cluster " +
        cluster_name + "?\nDATA ON EPHEMERAL DISKS WILL BE LOST, " +
        "BUT THE CLUSTER WILL KEEP USING SPACE ON\n" + 
        "AMAZON EBS IF IT IS EBS-BACKED!!\n" +
        "Stop cluster " + cluster_name + " (y/N): ")
    if response == "y":
      (main_res, subordinate_res, zoo_res) = get_existing_cluster(
          conn, opts, cluster_name)
      print "Stopping main..."
      for inst in main_res.instances:
        if inst.state not in ["shutting-down", "terminated"]:
          inst.stop()
      print "Stopping subordinates..."
      for inst in subordinate_res.instances:
        if inst.state not in ["shutting-down", "terminated"]:
          inst.stop()
      if zoo_res != None:
        print "Stopping zoo..."
        for inst in zoo_res.instances:
          if inst.state not in ["shutting-down", "terminated"]:
            inst.stop()

  elif action == "start":
    (main_res, subordinate_res, zoo_res) = get_existing_cluster(
        conn, opts, cluster_name)
    print "Starting subordinates..."
    for inst in subordinate_res.instances:
      if inst.state not in ["shutting-down", "terminated"]:
        inst.start()
    print "Starting main..."
    for inst in main_res.instances:
      if inst.state not in ["shutting-down", "terminated"]:
        inst.start()
    if zoo_res != None:
      print "Starting zoo..."
      for inst in zoo_res.instances:
        if inst.state not in ["shutting-down", "terminated"]:
          inst.start()
    wait_for_cluster(conn, main_res, subordinate_res, zoo_res)
    setup_cluster(conn, main_res, subordinate_res, zoo_res, opts, False)

  elif action == "shutdown":
    print >> stderr, ("The shutdown action is no longer available.\n" +
        "Use either 'destroy' to delete a cluster and all data on it,\n" +
        "or 'stop' to shut down the machines but have them persist if\n" +
        "you launched an EBS-backed cluster.")
    sys.exit(1)

  else:
    print >> stderr, "Invalid action: %s" % action
    sys.exit(1)


if __name__ == "__main__":
  logging.basicConfig()
  main()
