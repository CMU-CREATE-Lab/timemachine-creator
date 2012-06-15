#!/usr/bin/env ruby

$LOAD_PATH.unshift(File.expand_path(File.dirname(__FILE__)))

require "rexml/document"
require "date"
require "exifr"
require "json"
require "time"
 
capture_times = []
 
if ARGV.length < 2
  puts "Usage: ruby #{$0} path/to/image/files path/to/output/json"
  exit
end
 
unless File.exists?(ARGV[0])
  puts "Input directory/file does not exist. Please check the name you typed and try again."
  exit
end
 
unless File.exists?(ARGV[1])
  puts "Output json file does not exist. Please check the name you typed and try again."
  exit
end

if File.file?(ARGV[0])
  tmc_json = open(ARGV[0]) {|fh| JSON.load(fh)}
  tmc_json["source"]["capture_times"].flatten(1).each do |exif_date|
    begin
      capture_time = Time.at(exif_date).strftime("%Y-%m-%d %H:%M:%S")
    rescue
      capture_time = "NULL"
    end
    capture_times << capture_time
  end
else
  dir = ARGV[0].dup
  dir.chop! if dir[-1,1] == "/" || dir[-1,1] == "\\"
  path = File.expand_path("**/*.{JPG,jpg,tif,TIF}", dir)

  files = Dir.glob(path).sort
  files.each do |file|
    begin
      exifr_file = EXIFR::JPEG.new(file)
      if exifr_file.exif?
        exif_date = exifr_file.date_time.to_s
        if exif_date.nil? || exif_date.empty?
          exif_date = exifr_file.exif.date_time_original.to_s
        end
        capture_time = Time.parse(exif_date).strftime("%Y-%m-%d %H:%M:%S")
        #puts "Successfuly processed #{file}."
      else
        #puts "There was no exif data found in #{file}. NULL will be used for the capture time. Please go back and manually edit the output file in the end."
        capture_time = "NULL"
      end
    rescue
      #puts "There was an error parsing the capture time field in #{file}. NULL will be used for the capture time. Please go back and manually edit the output file in t$
      capture_time = "NULL"
    end
    capture_times << capture_time
  end
end

json = open(ARGV[1]) {|fh| JSON.load(fh)}
json["capture-times"] = capture_times
open(ARGV[1], "w") {|fh| fh.puts(JSON.pretty_generate(json))}
STDERR.puts "Successfully wrote capture times to #{ARGV[1]}"
