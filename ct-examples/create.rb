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
end

def setup
  clean
  FileUtils.cp_r "../datasets/carnival4_2x2_small", "carnival4_2x2_small.tmc/0100-unstitched"
end

def create
  setup
  sys_print "../ct/ct.rb carnival4_2x2_small.tmc carnival4_2x2_small.timemachine"
  sys_print "../ct/ct.rb carnival4_2x2_listed_small.tmc carnival4_2x2_listed_small.timemachine"
end

case ARGV[0]
when '--clean'
  clean
when '--setup'
  setup
else
  create
end
   
