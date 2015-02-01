#!/usr/bin/env ruby

$LOAD_PATH.unshift(File.expand_path(File.dirname(__FILE__)))
require "rexml/document"
require "json"
require "time"
require "exifr"

if ARGV.length < 2
  puts "Usage: ruby #{$0} path/to/image/files  path/to/output/json  [OPTIONS] -subsample-input #"
  exit 1
end

capture_times_src = ARGV[0]
output_json = ARGV[1]

unless File.exists?(capture_times_src)
  puts "Input directory/file does not exist. Please check the name you typed and try again."
  exit 1
end

unless File.exists?(output_json)
  puts "Output json file does not exist. Please check the name you typed and try again."
  exit 1
end

while !ARGV.empty?
  arg = ARGV.shift
  if arg == "-subsample-input"
    subsample_input = ARGV.shift.to_i
  elsif arg == "--print-milliseconds"
    print_milliseconds = true
  end
end

subsample_input ||= 1
capture_times = []

if File.file?(capture_times_src)
  tmc_json = open(capture_times_src) {|fh| JSON.load(fh)}
  tmc_json["source"]["capture_times"].flatten(1).each do |exif_date|
    begin
      extra = print_milliseconds ? ":%L" : ""
      capture_time = Time.at(exif_date).strftime("%Y-%m-%d %H:%M:%S#{extra}")
    rescue
      capture_time = "NULL"
    end
    capture_times << capture_time
  end
else
  files = (Dir.glob("#{capture_times_src}/*.{JPG,jpg,tif,TIF}") + Dir.glob("#{capture_times_src}/*/*.{JPG,jpg,tif,TIF}"))
  files = (subsample_input - 1).step(files.size - 1, subsample_input).map { |i| files[i] } if subsample_input > 1

  files.each do |file|
    begin
      exifr_file = EXIFR::JPEG.new(file)
      if exifr_file.exif?
        exif_date = exifr_file.date_time.to_s
        if exif_date.nil? || exif_date.empty?
          exif_date = exifr_file.exif.date_time_original.to_s
        end
        extra = print_milliseconds ? ":%L" : ""
        capture_time = Time.parse(exif_date).strftime("%Y-%m-%d %H:%M:%S#{extra}")
        #puts "Successfuly processed #{file}."
      else
        #puts "There was no exif data found in #{file}. NULL will be used for the capture time. Please go back and manually edit the output file in the end."
        capture_time = "NULL"
      end
    rescue
      #puts "There was an error parsing the capture time field in #{file}. NULL will be used for the capture time. Please go back and manually edit the output json file at #{output_json}"
      capture_time = "NULL"
    end
    capture_times << capture_time
  end
end

json = open(output_json) {|fh| JSON.load(fh)}
json["capture-times"] = capture_times.sort
open(output_json, "w") {|fh| fh.puts(JSON.pretty_generate(json))}
STDERR.puts "Successfully wrote capture times to #{output_json}"
