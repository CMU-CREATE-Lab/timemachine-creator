#!/usr/bin/env ruby

$LOAD_PATH.unshift(File.expand_path(File.dirname(__FILE__)))
require 'json.rb'
require 'rexml/document'
require 'time'

capture_times = []

if ARGV.length < 2
  puts "Usage: ruby #{$0} path/to/.gigapans path/to/.json"
  exit 1
end

unless File.exists?(ARGV[0])
  puts "Input directory does not exist. Please check the name you typed and try again."
  exit 1
end

unless File.exists?(ARGV[1])
  puts "Output json file does not exist. Please check the name you typed and try again."
  exit 1
end

dir = ARGV[0].dup
dir.chop! if dir[-1,1] == "/" || dir[-1,1] == "\\"
path = File.expand_path('*.gigapan', dir)

files = Dir.glob(path).sort
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
  #puts "Successfuly processed #{file}."
end

json = open(ARGV[1]) {|fh| JSON.load(fh)}
json["capture-times"] = capture_times
open(ARGV[1], "w") {|fh| fh.puts(JSON.pretty_generate(json))}
puts "Successfully wrote capture times to #{ARGV[1]}"
