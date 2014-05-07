#!/usr/bin/ruby

require 'fileutils'

Dir.chdir File.dirname(__FILE__)

def sys_print cmd
  STDERR.puts cmd
  system cmd or raise "'#{cmd}' failed"
end

def clean
  FileUtils.rm_rf Dir.glob("*/0???-*")
  FileUtils.rm_rf Dir.glob("*.timemachine")
  FileUtils.rm_rf Dir.glob("job*")
end

def setup
  clean
  FileUtils.cp_r "../../datasets/carnival4_2x2", "carnival4_2x2.tmc/0100-unstitched"
end

def create
  setup
  sys_print "../../ct/ct.rb --remote ../../remote-cluster-scripts/run_remote --remote-json -j 8 -r 10 carnival4_2x2.tmc carnival4_2x2.timemachine"
end

case ARGV[0]
when '--clean'
  clean
when '--setup'
  setup
else
  create
end

