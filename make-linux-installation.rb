#!/usr/bin/ruby

require 'fileutils'

def usage
  STDERR.puts "usage: make-linux-installation.rb majorversion.minorversion"
  exit 1
end

def sys_print cmd
  STDERR.puts cmd
  system cmd or raise "'#{cmd}' failed"
end

if ARGV.size != 1
  usage
end

version = ARGV.shift

if ! (version =~ /^\d+\.\d+$/)
  usage
end

dest = "tmc-linux-#{version}"
sys_print "rm -rf #{dest}"
sys_print "mkdir -p #{dest} #{dest}/tmclib #{dest}/ctlib #{dest}/ct-examples"
# TODO: add tmc/debug/tmc when we're building this
sys_print "cp -a tilestacktool/tilestacktool tilestacktool/ffmpeg/linux/ffmpeg ct/ct.rb time-machine-explorer #{dest}"
sys_print "cp -a tmc/assets tmc/count_to_10.rb tmc/index.html tmc/js tmc/test.html #{dest}/tmclib"
sys_print "cp -a ct/backports ct/backports.rb ct/g10.response ct/image_size.rb ct/json ct/json.rb ct/tile.rb ct/tileset.rb ct/xmlsimple.rb #{dest}/ctlib"

sys_print "ct-examples/create.rb --setup"
sys_print "cp -a ct-examples/carnival4_2x2_small.tmc #{dest}/ct-examples"
open("#{dest}/ct-examples/create.sh","w") do |out| 
  out.puts "#!/bin/sh"
  out.puts "../ct.rb carnival4_2x2_small.tmc carnival4_2x2_small.timemachine"
end
sys_print "chmod a+x #{dest}/ct-examples/create.sh"

sys_print "#{dest}/tilestacktool --selftest"
sys_print "#{dest}/ct.rb --selftest"

sys_print "tar cfz #{dest}.tgz #{dest}"
sys_print "ls -lh #{dest}.tgz"

