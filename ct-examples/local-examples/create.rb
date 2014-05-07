#!/usr/bin/env ruby

require 'fileutils'

is_windows = (RUBY_PLATFORM  =~ (/mswin|mingw|cygwin/))
$ruby_call = is_windows ? "ruby" : ""

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
  FileUtils.cp_r "../../datasets/carnival4_2x2", "carnival4_2x2.tmc/0100-unstitched"
end

def create
  setup
  sys_print "#{$ruby_call} ../../ct/ct.rb carnival4_2x2.tmc carnival4_2x2.timemachine"
  sys_print "#{$ruby_call} ../../ct/ct.rb patp10_1x1_listed.tmc patp10_1x1_listed.timemachine"
end

case ARGV[0]
when '--clean'
  clean
when '--setup'
  setup
else
  create
end
