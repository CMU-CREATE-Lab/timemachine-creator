#!/usr/bin/env ruby

$LOAD_PATH.unshift(File.expand_path(File.dirname(__FILE__)))
require "rexml/document"
require "json"
require "time"

if ARGV.length < 2
  puts "Usage: ruby #{$0} path/to/.gigapans  path/to/.json  [OPTIONS] -subsample-input #"
  exit 1
end

capture_times_src = ARGV[0]
output_json = ARGV[1]

unless File.exists?(capture_times_src)
  puts "Input directory does not exist. Please check the name you typed and try again."
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
  end
end

subsample_input ||= 1
capture_times = []

dir = capture_times_src.dup
dir.chop! if dir[-1,1] == "/" || dir[-1,1] == "\\"
path = File.expand_path('*.gigapan', dir)

files = Dir.glob(path).sort
files = (subsample_input - 1).step(files.size - 1, subsample_input).map { |i| files[i] } if subsample_input > 1

files.each do |file|
  # Read in the .gigapan file
  gigapan = REXML::Document.new File.new(file)

  # Grab the notes section
  notes = gigapan.elements.to_a("GigapanStitcher/notes")
  if notes.length == 0
    puts "#{file} did not have a notes section. NULL will be used for the capture time. Please go back and manually edit the output file in the end."
    capture_times << "NULL"
    next
  end

  # Extract the capture time range from the notes section
  notes_array = notes[0].to_s.split("\n")
  capture_time_index = nil
  notes_array.each_with_index do |x,i|
    if x.include?("Capture time:")
      capture_time_index = i
      break
    end
  end

  if capture_time_index.nil?
    puts "The notes section in #{file} did not have a capture time field. NULL will be used for the capture time. Please go back and manually edit the output file in the end."
    capture_times << "NULL"
    next
  end
  begin
    capture_time_range = notes_array[capture_time_index].scan(/\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2} - \d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}/)[0].split(" - ")
    capture_time_start = capture_time_range[0].strip
    capture_time_end = capture_time_range[1].strip
  rescue
    puts "There was an error parsing the capture time field in #{file}. NULL will be used for the capture time. Please go back and manually edit the output file in the end."
    capture_times << "NULL"
    next
  end

  capture_times << capture_time_start
end

json = open(output_json) {|fh| JSON.load(fh)}
json["capture-times"] = capture_times
open(output_json, "w") {|fh| fh.puts(JSON.pretty_generate(json))}
STDERR.puts "Successfully wrote capture times to #{output_json}"
