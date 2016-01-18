# Copyright (c) 2014 The Chromium Embedded Framework Authors. All rights
# reserved. Use of this source code is governed by a BSD-style license that
# can be found in the LICENSE file

from exec_util import exec_cmd
import os

def is_ue_fork(path):
  return os.path.exists(os.path.join(path, 'UE_VENDOR_REVISION.txt'))

def read_ue_fork_file(path):
  """ Reads the UE_VENDOR_REVISION file. """
  if os.path.exists(os.path.join(path, 'UE_VENDOR_REVISION.txt')):
    fp = open(os.path.join(path, 'UE_VENDOR_REVISION.txt'), 'r')
    data = fp.read()
    fp.close()
  else:
    raise Exception("Path does not exist: %s" % (os.path.join(path, 'UE_VENDOR_REVISION.txt')))

  # Parse the contents.
  return eval(data, {'__builtins__': None}, None)


def is_checkout(path):
  """ Returns true if the path represents a git checkout or a ue fork. """
  return os.path.exists(os.path.join(path, '.git')) or is_ue_fork(path)

def get_hash(path = '.', branch = 'HEAD'):
  """ Returns the git hash for the specified branch/tag/hash. """
  if is_ue_fork(path):
    return read_ue_fork_file(path)['commit']
  else:
    cmd = "git rev-parse %s" % branch
    result = exec_cmd(cmd, path)
    if result['out'] != '':
      return result['out'].strip()
    return 'Unknown'

def get_url(path = '.'):
  """ Returns the origin url for the specified path. """
  if is_ue_fork(path):
    return read_ue_fork_file(path)['url']
  else:
    cmd = "git config --get remote.origin.url"
    result = exec_cmd(cmd, path)
    if result['out'] != '':
      return result['out'].strip()
    return 'Unknown'

def get_commit_number(path = '.', branch = 'HEAD'):
  """ Returns the number of commits in the specified branch/tag/hash. """
  if is_ue_fork(path):
    return read_ue_fork_file(path)['revision']
  else:
    cmd = "git rev-list --count %s" % branch
    result = exec_cmd(cmd, path)
    if result['out'] != '':
      return result['out'].strip()
    return '0'

def get_changed_files(path = '.'):
  """ Retrieves the list of changed files. """
  # not implemented
  return []
