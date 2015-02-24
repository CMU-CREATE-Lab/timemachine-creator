#!/usr/bin/env ruby

# You need the xml-simple gem to run this script
# [sudo] gem install xml-simple

# Ruby standard modules
require 'fileutils'
require 'open-uri'
require 'set'
require 'thread'

# Local modules
$LOAD_PATH.unshift(File.expand_path(File.dirname(__FILE__)))
$LOAD_PATH.unshift(File.expand_path(File.dirname(__FILE__)) + "/ctlib")
require 'backports'
require 'image_size'
require 'json'
require 'tile'
require 'tileset'
require 'xmlsimple'
require 'exifr'

$ajax_includes = {}

def include_ajax(filename, contents)
  $ajax_includes[filename] = contents
end

def include_ajax_file(filename)
  include_ajax filename, Filesystem.read_file("#{$explorer_source_dir}/#{filename}")
end

def javascript_quote x
  # JSON.generate works for arrays and maps, but not strings or numbers.  So wrap in an array
  # before generating, and then take the square brackets off
  JSON.generate([x])[1...-1]
end

def ajax_includes_to_javascript
  statements = ["cached_ajax={};"]
  $ajax_includes.keys.sort.each do |filename|
    statements << "cached_ajax['#{filename}']=#{javascript_quote($ajax_includes[filename])};"
  end
  statements.join "\n\n"
end

#profile = "ct.profile.13n.txt"
profile = false
debug = false
$check_mem = false

class Partial
  def initialize(n, total)
    @n = n
    @total = total
  end

  def self.none
    Partial.new(0,1)
  end

  def self.all
    Partial.new(1,1)
  end

  def none?
    @n == 0
  end

  def all?
    @total == 1 && !none?
  end

  def apply(seq)
    none? and return []
    seq = seq.to_a
    len = seq.size
    start = len * (@n - 1) / @total
    finish = len * @n / @total
    start...finish
  end
  def to_s
    if none?
      "Skip all"
    elsif all?
      "All"
    else
      "Part #{@n} of #{@total}"
    end
  end
end

$compute_tilestacks = Partial.all
$compute_videos = Partial.all
$preserve_source_tiles = false
$skip_trailer = false
$skip_leader = false
$sort_by_exif_dates = false

if profile
  require 'rubygems'
  require 'ruby-prof'
  # RubyProf.measure_mode = RubyProf::MEMORY
  RubyProf.start
end

if $check_mem
  require 'check-mem'
end

start_time = Time.new

if debug
  require 'ruby-debug'
end

$run_remotely = nil
$run_remotely_json = false
$run_remotely_json_dir = nil
$cache_folder = nil
$dry_run = false

ruby_version = RUBY_VERSION.split('.')[0,3].join.to_i

if ruby_version <= 185 || ruby_version == 191 || ruby_version == 192
  raise "Ruby version is #{RUBY_VERSION}, but must be >= 1.8.6, and also must not be 1.9.1 or 1.9.2 because of threading bugs in those versions."
end

if `echo %PATH%`.chomp != '%PATH%'
  $os = 'windows'
elsif File.exists?('/proc')
  $os = 'linux'
else
  $os = 'osx'
end

if $os == 'windows'
  require 'win32/registry'
  require File.join(File.dirname(__FILE__), 'shortcut')
  require 'filesystem'
end

def temp_file_unique_fragment
  "tmp-#{Process.pid}-#{Thread.current.object_id}-#{Time.new.to_i}"
end

def self_test(stitch = true)
  success = true
  STDERR.puts "Testing tilestacktool...\n"
  status = Kernel.system(*(tilestacktool_cmd + ['--selftest']))

  if status
    STDERR.puts "tilestacktool: succeeded\n"
  else
    STDERR.puts "tilestacktool: FAILED\n"
    success = false
  end

  if stitch
    STDERR.puts "Testing stitch..."
    status = Kernel.system($stitch, '--batch-mode', '--version')
    if status
      STDERR.puts "stitch: succeeded"
    else
      STDERR.puts "stitch: FAILED"
      success = false
    end
  end

  if $explorer_source_dir
    STDERR.puts "Explorer source dir: succeeded"
  else
    STDERR.puts "Explorer source dir not found: FAILED"
  end

  STDERR.puts "ct.rb self-test #{success ? 'succeeded' : 'FAILED'}."

  return success
end

def read_from_registry(hkey,subkey)
  begin
    reg_type = Win32::Registry::Constants::KEY_READ | KEY_WOW64_64KEY
    reg = Win32::Registry.open(hkey,subkey,reg_type)
    return reg
  rescue Win32::Registry::Error
    # If we fail to read the key (e.g. it does not exist, we end up here)
    return nil
  end
end

$tilestacktool_args = []

def tilestacktool_cmd
  [$tilestacktool] + $tilestacktool_args
end

def stitch_cmd
  cmd = [$stitch]
  cmd << '--batch-mode'
  cmd << '--nolog'
  $os == 'linux' and $run_remotely and cmd << '--xvfb'
  cmd
end

$sourcetypes = {}

class Filesystem
  def self.mkdir_p(path)
    FileUtils.mkdir_p path
  end

  def self.read_file(path)
    open(path, 'r') { |fh| fh.read }
  end

  def self.write_file(path, data)
    open(path, 'w') { |fh| fh.write data }
  end

  def self.cache_directory(root)
  end

  def self.exists?(path)
    File.exists? path
  end

  def self.cached_exists?(path)
    File.exists? path
  end

  def self.cp_r(src, dest)
    FileUtils.cp_r src, dest
  end

  def self.cp(src, dest)
    FileUtils.cp src, dest
  end

  def self.mv(src, dest)
    FileUtils.mv src, dest
  end

  def self.rm(path)
    FileUtils.rm_rf Dir.glob("#{path}")
  end

end

def write_json_and_js(path, prefix, obj)
  Filesystem.mkdir_p File.dirname(path)
  js_path = path + '.js'
  Filesystem.write_file(js_path, prefix + '(' + JSON.pretty_generate(obj) + ')')
  STDERR.puts("Wrote #{js_path}")

  # Deprecated.  TODO(RS): Remove this soon
  write_json_path = true

  if write_json_path
    json_path = path + '.json'
    Filesystem.write_file(json_path, JSON.pretty_generate(obj))
    STDERR.puts("Wrote #{json_path}")
  end
end

Dir.glob(File.dirname(__FILE__) + "/*_source.rb").each do |file|
  STDERR.puts "Loading #{File.basename(file)}..."
  require file
end

$verbose = false
@@global_parent = nil

#
# DOCUMENTATION IS IN README_CT.TXT
#

def date
  Time.now.strftime("%Y-%m-%d %H:%M:%S")
end

def usage(msg=nil)
  msg and STDERR.puts msg
  STDERR.puts "usage:"
  STDERR.puts "ct.rb [flags] input.tmc output.timemachine"
  STDERR.puts "Flags:"
  STDERR.puts "-j N:  number of parallel jobs to run (default 1)"
  STDERR.puts "-r N:  max number of rules per job (default 1)"
  STDERR.puts "-v:    verbose"
  STDERR.puts "--remote [script]:  Use script to submit remote jobs"
  STDERR.puts "--remote_json [script]:  Use script to submit remote jobs"
  STDERR.puts "--tilestacktool path: full path of tilestacktool"
  STDERR.puts "--cache_folder path: Path of cache folder. Usefull in cluster mode"
  exit 1
end

def without_extension(filename)
  filename[0..-File.extname(filename).size-1]
end

class TemporalFragment
  attr_reader :start_frame, :end_frame, :fragment_seq
  # start_frame and end_frame are both INCLUSIVE
  def initialize(start_frame, end_frame, fragment_seq = nil)
    @start_frame = start_frame
    @end_frame = end_frame
    @fragment_seq = fragment_seq
  end
end

class VideoTile
  attr_reader :c, :r, :level, :x, :y, :subsample, :fragment
  def initialize(c, r, level, x, y, subsample, fragment)
    @c = c
    @r = r
    @level = level
    @x = x
    @y = y
    @subsample = subsample
    @fragment = fragment
  end

  def path
    ret = "#{@level}/#{@r}/#{@c}"
    @fragment.fragment_seq and ret += "_#{@fragment.fragment_seq}"
    ret
  end

  def to_s
    "#<VideoTile path='#{path}' r=#{@r} c=#{@c} level=#{@level} x=#{@x} y=#{@y} subsample=#{@subsample}>"
  end

  def source_bounds(vid_width, vid_height)
    {'xmin' => @x, 'ymin' => @y, 'width' => vid_width * @subsample, 'height' => vid_height * @subsample}
  end

  def frames
    {'start' => @fragment.start_frame, 'end' => @fragment.end_frame}
  end
end

class Rule
  attr_reader :targets, :dependencies, :commands, :local
  @@all = []
  @@all_targets = Set.new
  @@n = 0

  def self.clear_all
    @@all = []
  end

  def self.all
    @@all
  end

  def self.has_target?(target)
    @@all_targets.member? target
  end

  def array_of_strings?(a)
    a.class == Array && a.all? {|e| e.class == String}
  end

  def initialize(targets, dependencies, commands, options={})
    if $check_mem
      @@n += 1
      if @@n % 10000 == 0
        CheckMem.logvm("Created #{@@n} rules")
      end
    end

    if targets.class == String
      targets = [targets]
    end
    commands = commands.map {|cmd| cmd.map {|x| x.class == Hash ? JSON.generate(x) : x.to_s} }
    array_of_strings?(targets) or raise "targets must be an array of pathnames"
    array_of_strings?(dependencies) or raise "dependencies must be an array of pathnames"
    @targets = targets
    @dependencies = dependencies
    @commands = commands
    @@all << self
    targets.each { |target| @@all_targets << target }
  end

  def self.add(targets, dependencies, commands, options={})
    Rule.new(targets, dependencies, commands, options).targets
  end

  def self.touch(target, dependencies)
    Rule.add(target, dependencies, [tilestacktool_cmd + ['--createfile', target]], {:local => true})
  end

  def to_make
    ret = "#{@targets.join(" ")}: #{@dependencies.join(" ")}\n"
    if @commands.size
      ret += "\t@$(SS) '#{@commands.join(" && ")}'"
    end
    ret
  end

  def to_s
    "[Rule: #{targets.join(" ")}]"
  end
end

class VideosetCompiler
  attr_reader :videotype, :vid_width, :vid_height, :overlap_x, :overlap_y, :compression, :label, :fps, :show_frameno, :parent
  attr_reader :id

  # DEPRECATED
  # We now allow users to pass in an array containing a desired width and height
  @@sizes = {
    "large" => {:vid_size=>[1088,624], :overlap=>[1088/4, 624/4]},
    "small" => {:vid_size=>[768,432], :overlap=>[768/3, 432/3]}
  }

  @@videotypes = {
    "h.264" => "mp4",
    "vp8" => "webm",
    "proreshq" => "mov"
  }

  def initialize(parent, settings)
    @parent = parent
    @@global_parent = parent
    @label = settings["label"] || raise("Video settings must include label")

    @videotype = settings["type"] || raise("Video settings must include type")
    @@videotypes.member?(@videotype) || raise("Video type must be one of the following codecs [#{@@videotypes.keys.join(", ")}].")
    @videotype_container = @@videotypes[@videotype]

    size = settings["size"] || raise("Video settings must include size")

    if size.kind_of?(Array)
      overlap = settings["overlap"] || 0.25
      (@vid_width, @vid_height) = [(size[0] / (1-overlap)).ceil, (size[1] / (1-overlap)).ceil]

      # Ensure that the width and height are multiples of 2 for ffmpeg
      @vid_width = ((@vid_width - 1) / 2 + 1) * 2
      @vid_height = ((@vid_height - 1) / 2 + 1) * 2

      (@overlap_x, @overlap_y) = [@vid_width - size[0], @vid_height - size[1]]
    else # Backwards compatibility
      @@sizes.member?(size) || raise("Video size must be one of [#{@@sizes.keys.join(", ")}]. WARNING: This way of declaring sizes is deprecated.")
      (@vid_width, @vid_height) = @@sizes[size][:vid_size]
      (@overlap_x, @overlap_y) = @@sizes[size][:overlap]
    end

    (@compression = settings["compression"] || settings["quality"]) or raise "Video settings must include compression"
    @compression = @compression.to_i
    @compression > 0 || raise("Video compression must be > 0")

    @fps = settings["fps"] || raise("Video settings must include fps")
    @fps = @fps.to_f
    @fps > 0 || raise("Video fps must be > 0")

    @show_frameno = settings["show_frameno"]

    @frames_per_fragment = settings["frames_per_fragment"]

    @leader = @frames_per_fragment || $skip_leader ? 0 : compute_leader_length

    initialize_videotiles
    initialize_id
  end

  def compute_leader_length
    leader_bytes_per_pixel = {
      30 => 2701656.0 / (@vid_width * @vid_height * 90),
      28 => 2738868.0 / (@vid_width * @vid_height * 80),
      26 => 2676000.0 / (@vid_width * @vid_height * 70),
      24 => 2556606.0 / (@vid_width * @vid_height * 60)
    }

    # TODO:
    # If we do not have a calculated leader for the requested compression,
    # default to either the min or max depending upon the value.
    compression_lookup = @compression
    if not leader_bytes_per_pixel.member?(@compression)
      @compression < 24 ? compression_lookup = 24 : compression_lookup = 30
    end

    bytes_per_frame = @vid_width * @vid_height * leader_bytes_per_pixel[compression_lookup]

    leader_threshold = 1200000
    estimated_video_size = bytes_per_frame * nframes
    if estimated_video_size < leader_threshold
      # No leader needed
      return 0
    end

    minimum_leader_length = 2500000
    leader_nframes = minimum_leader_length / bytes_per_frame

    # Round up to nearest multiple of frames per keyframe
    frames_per_keyframe = 10
    leader_nframes = (leader_nframes / frames_per_keyframe).ceil * frames_per_keyframe

    return leader_nframes
  end

  def initialize_id
    tokens = ["crf#{@compression}", "#{@fps.round}fps"]
    (@leader > 0) && tokens << "l#{@leader}"
    tokens << "#{@vid_width}x#{@vid_height}"
    @id = tokens.join('-')
  end

  def nframes
    @parent.source.framenames.size
  end

  def initialize_videotiles
    $compute_videos or return
    # Compute levels
    levels = []
    @levelinfo = []
    subsample = 1
    while true do
      input_width = @vid_width * subsample
      input_height = @vid_height * subsample

      levels << {:subsample => subsample, :input_width => input_width, :input_height => input_height}

      if input_width >= @parent.source.width and input_height >= @parent.source.height
        break
      end
      subsample *= 2
    end

    levels.reverse!
    for level in 0...levels.size do
      levels[level][:level] = level
    end

    if @frames_per_fragment
      @temporal_fragments = (nframes / @frames_per_fragment).floor.times.map do |i|
        TemporalFragment.new(i * @frames_per_fragment,
                             [(i + 1) * @frames_per_fragment, nframes].min - 1,
                             i)
      end
    else
      @temporal_fragments = [TemporalFragment.new(0, nframes - 1)]
    end

    @videotiles = []

    levels.each do |level|
      level_rows = 1+((@parent.source.height - level[:input_height]).to_f / (@overlap_y * level[:subsample])).ceil
      level_rows = [1,level_rows].max
      level_cols = 1+((@parent.source.width - level[:input_width]).to_f / (@overlap_x * level[:subsample])).ceil
      level_cols = [1,level_cols].max
      @levelinfo << {"rows" => level_rows, "cols" => level_cols}
      #puts "** level=#{level[:level]} subsample=#{level[:subsample]} #{level_cols}x#{level_rows}=#{level_cols*level_rows} videos input_width=#{level[:input_width]} input_height=#{level[:input_height]}"

      rows_to_compute = $compute_videos.apply(0...level_rows)
      rows_to_compute.each do |r|
        y = r * @overlap_y * level[:subsample]
        level_cols.times do |c|
          x = c * @overlap_x * level[:subsample]
          @temporal_fragments.each do |fragment|
            @videotiles << VideoTile.new(c, r, level[:level], x, y, level[:subsample], fragment)
          end
        end
      end
    end
  end

  def rules(dependencies)
    if not $compute_videos
      STDERR.puts "#{id}: skipping video creation"
      dependencies
    else
      STDERR.puts "#{id}: #{@videotiles.size} videos (#{$compute_videos})"
      @videotiles.flat_map do |vt|
        target = "#{@parent.videosets_dir}/#{id}/#{vt.path}.#{@videotype_container}"
        cmd = tilestacktool_cmd
        cmd << "--create-parent-directories"

        cmd << '--path2stack'
        cmd += [@vid_width, @vid_height]
        frames = {'frames' => vt.frames,
                  'bounds' => vt.source_bounds(@vid_width, @vid_height)}
        cmd << JSON.generate(frames)
        cmd << @parent.tilestack_dir

        cmd += @parent.video_filter || []

        @leader > 0 and cmd += ["--prependleader", @leader]

        cmd += ["--blackstack",
                10, # number of frames
                @vid_width,
                @vid_height,
                3,  # bands per pixel
                8   # bits per band
                ] unless $skip_trailer

        cmd << "--cat"

        cmd += ['--writevideo', target, @fps, @compression, @videotype]

        Rule.add(target, dependencies, [cmd])
      end
    end
  end

  def info
    ret = {
      "level_info"   => @levelinfo,
      "nlevels"      => @levelinfo.size,
      "level_scale"  => 2,
      "frames"       => nframes,
      "fps"          => @fps,
      "leader"       => @leader,
      "tile_width"   => @overlap_x,
      "tile_height"  => @overlap_y,
      "video_width"  => @vid_width,
      "video_height" => @vid_height,
      "width"        => parent.source.width,
      "height"       => parent.source.height
    }
    @frames_per_fragment and ret['frames_per_fragment']=@frames_per_fragment
    ret
  end

  def write_json
    write_json_and_js("#{@parent.videosets_dir}/#{id}/r", 'org.cmucreatelab.loadVideoset', info)
    include_ajax "./#{id}/r.json", info
  end
end

# Looks for images in one or two levels below dir, and sorts them alphabetically (or by exif time stamps if the user specifies this)
# Supports Windows shortcuts
def find_images_in_directory(dir)
  valid_image_extensions = Set.new [".jpg",".jpeg",".png",".tif",".tiff", ".raw", ".kro", ".lnk"]
  images = []
  dir_list = Dir.glob("#{dir}/*.*") + Dir.glob("#{dir}/*/*.*")

  # It is possible that image numbering wrapped and thus lower numbers should come after larger ones. Thus we instead sort by exif time stamps.
  # Else sort alphabetically
  if $sort_by_exif_dates
    dir_list = dir_list.sort_by { |filename| EXIFR::JPEG.new(filename).date_time.to_i }
  else
    dir_list = dir_list.sort
  end

  # Subsample input list if needed
  dir_list = (@subsample_input - 1).step(dir_list.size - 1, @subsample_input).map { |i| dir_list[i] } if @subsample_input > 1

  dir_list.each do |image|
    next unless valid_image_extensions.include? File.extname(image).downcase
    if $os == 'windows' && File.extname(image) == ".lnk"
      images << Win32::Shortcut.open(image).path
    else
      images << image
    end
  end
  images
end

class ImagesSource
  attr_reader :ids, :width, :height, :tilesize, :tileformat, :subsample, :raw_width, :raw_height
  attr_reader :capture_times, :capture_time_parser, :capture_time_parser_inputs, :framenames, :subsample_input, :capture_time_print_milliseconds

  def initialize(parent, settings)
    @parent = parent
    @@global_parent = parent
    @image_dir="#{@parent.store}/0100-original-images"
    @raw_width = settings["width"]
    @raw_height = settings["height"]
    @subsample = settings["subsample"] || 1
    @subsample_input = settings["subsample_input"] || 1
    @images = settings["images"] ? settings["images"].flatten : nil
    @capture_times = settings["capture_times"] ? settings["capture_times"].flatten : nil
    @capture_time_parser = settings["capture_time_parser"] ? File.expand_path(settings["capture_time_parser"]) : "#{File.dirname(__FILE__)}/extract_exif_capturetimes.rb"
    @capture_time_parser_inputs = settings["capture_time_parser_inputs"] || "#{@parent.store}/0100-original-images/"
    @capture_time_print_milliseconds = settings["capture_time_print_milliseconds"] || false
    initialize_images
    initialize_framenames
    @tilesize = settings["tilesize"] || ideal_tilesize(@framenames.size)
    @tileformat = settings["tileformat"] || "kro"
  end

  def ideal_tilesize(nframes)
    # Aim for less than half a gigabyte, but no more than 512
    tilesize = 512
    while tilesize * tilesize * nframes * 3 > 0.5e9
      tilesize /= 2
    end
    tilesize
  end

  def initialize_images
    @images ||= find_images_in_directory(@image_dir)
    @images.empty? and usage "No images specified, and none found in #{@image_dir}"
    @images.map! {|image| File.expand_path(image, @parent.store)}
    @raw = (File.extname(@images[0]).downcase == ".raw")
    initialize_size
  end

  def initialize_size
    if @raw
      @raw_width and @raw_height or usage("Must specify width and height for .raw image source")
      @width = @raw_width
      @height = @raw_height
    else
      open(@images[0], "rb") do |fh|
        (@width, @height)=ImageSize.new(fh).get_size
      end
    end
    @width /= @subsample
    @height /= @subsample
  end

  def initialize_framenames
    frames = @images.map {|filename| File.expand_path(without_extension(filename)).split('/')}
    # Remove common prefixes
    while frames[0].length > 1 && frames.map {|x|x[0]}.uniq.length == 1
      frames = frames.map {|x| x[1..-1]}
    end
    @framenames = frames.map {|frame| frame.join('@')}
    @image_framenames = {}
    @framenames.size.times {|i| @image_framenames[@images[i]] = @framenames[i]}
  end

  def image_to_tiles_rule(image)
    fn = @image_framenames[image]
    target = "#{@parent.tiles_dir}/#{fn}.data/tiles"
    cmd = tilestacktool_cmd + ['--tilesize', @tilesize, '--image2tiles', target, @tileformat, image]
    Rule.add(target, [image], [cmd])
  end

  def tiles_rules
    @images.flat_map {|image| image_to_tiles_rule(image)}
  end

end

$sourcetypes['images'] = ImagesSource

class GigapanOrgSource
  attr_reader :ids, :width, :height, :tilesize, :tileformat, :framenames, :subsample
  attr_reader :capture_times, :capture_time_parser, :capture_time_parser_inputs, :subsample_input

  def initialize(parent, settings)
    @parent = parent
    @@global_parent = parent
    @urls = settings["urls"]
    @ids = @urls.map{|url| id_from_url(url)}
    @subsample = settings["subsample"] || 1
    @subsample_input = settings["subsample_input"] || 1
    @capture_times = settings["capture_times"] ? settings["capture_times"].flatten : nil
    @capture_time_parser = settings["capture_time_parser"] ? File.expand_path(settings["capture_time_parser"]) : "#{File.dirname(__FILE__)}/extract_gigapan_capturetimes.rb"
    @capture_time_parser_inputs = settings["capture_time_parser_inputs"] || "#{@parent.store}/0200-tiles"
    @tileformat = "jpg"
    initialize_dimensions
  end

  def initialize_dimensions
    @tilesize = 256

    id = @ids[0]
    api_url = "http://api.gigapan.org/beta/gigapans/#{id}.json"
    gigapan = open(api_url) { |fh| JSON.load(fh) }
    @width = gigapan["width"] or raise "Gigapan #{id} has no width"
    @width = @width.to_i / @subsample
    @height = gigapan["height"] or raise "Gigapan #{id} has no height"
    @height = @height.to_i / @subsample
    @framenames = (0...@ids.size).map {|i| framename(i)}
  end

  def framename(i)
    "#{"%06d"%i}-#{@ids[i]}"
  end

  def tiles_rules
    @ids.size.times.flat_map do |i|
      target = "#{@parent.tiles_dir}/#{framename(i)}.data/tiles"
      Rule.add(target, [], [['mirror-gigapan.rb', @ids[i], target]])
    end
  end

  def id_from_url(url)
    url.match(/(\d+)/) {|id| return id[0]}
    raise "Can't find ID in url #{url}"
  end
end

$sourcetypes['gigapan.org'] = GigapanOrgSource

class PrestitchedSource
  attr_reader :ids, :width, :height, :tilesize, :tileformat, :framenames, :subsample
  attr_reader :capture_times, :capture_time_parser, :capture_time_parser_inputs, :subsample_input

  def initialize(parent, settings)
    @parent = parent
    @@global_parent = parent
    @subsample = settings["subsample"] || 1
    @subsample_input = settings["subsample_input"] || 1
    @capture_times = settings["capture_times"] ? settings["capture_times"].flatten : nil
    @capture_time_parser = settings["capture_time_parser"] ? File.expand_path(settings["capture_time_parser"]) : "#{File.dirname(__FILE__)}/extract_gigapan_capturetimes.rb"
    @capture_time_parser_inputs = settings["capture_time_parser_inputs"] || "#{@parent.store}/0200-tiles"
    initialize_frames
  end

  def initialize_frames
    @framenames = Dir.glob("#{@parent.store}/0200-tiles/*.data").map {|dir| File.basename(without_extension(dir))}.sort

    data = XmlSimple.xml_in("#{@parent.store}/0200-tiles/#{framenames[0]}.data/tiles/r.info")
    @width =
      data["bounding_box"][0]["bbox"][0]["max"][0]["vector"][0]["elt"][0].to_i -
      data["bounding_box"][0]["bbox"][0]["min"][0]["vector"][0]["elt"][0].to_i
    @height =
      data["bounding_box"][0]["bbox"][0]["max"][0]["vector"][0]["elt"][1].to_i -
      data["bounding_box"][0]["bbox"][0]["min"][0]["vector"][0]["elt"][1].to_i
    @tilesize = data["tile_size"][0].to_i
    @tileformat = "jpg"

    @width /= @subsample
    @height /= @subsample
  end

  def tiles_rules
    []
  end
end

$sourcetypes['prestitched'] = PrestitchedSource

class StitchSource
  attr_reader :ids, :width, :height, :tilesize, :tileformat, :framenames, :subsample
  attr_reader :align_to, :stitcher_args, :camera_response_curve, :cols, :rows, :rowfirst
  attr_reader :directory_per_position, :capture_times, :capture_time_parser, :capture_time_parser_inputs
  attr_reader :master_gigapan, :subsample_input

  def initialize(parent, settings)
    @parent = parent
    @@global_parent = parent
    @subsample = settings["subsample"] || 1
    @subsample_input = settings["subsample_input"] || 1
    @width = settings["width"] || 1
    @height = settings["height"] || 1
    unless settings["master_gigapan"]
      @align_to = settings["align_to"] or raise "Must include align-to in 'source' section of definition.tmc"
    else
      @align_to = []
      @master_gigapan = settings["master_gigapan"]
      File.exists?(@master_gigapan) or raise "Can't find the master align .gigapan file at path: #{@master_gigapan}"
    end
    #settings["align_to_comment"] or raise "Must include align-to-comment"
    @stitcher_args = settings["stitcher_args"] || ""
    @camera_response_curve = settings["camera_response_curve"]
    unless @camera_response_curve
      response_curve_path = [File.expand_path("#{File.dirname(__FILE__)}/g10.response"), File.expand_path("#{File.dirname(__FILE__)}/ctlib/g10.response")]
      @camera_response_curve = response_curve_path.find {|file| File.exists? file}
      @camera_response_curve or raise "Can't find camera response curve in search path #{response_curve_path.join(':')}"
    else
      File.exists?(@camera_response_curve) or raise "Camera response curve set to #{@camera_response_curve} but the file doesn't exist"
    end
    @cols = settings["cols"] or raise "Must include cols in 'source' section of definition.tmc"
    @rows = settings["rows"] or raise "Must include rows in 'source' section of definition.tmc"
    @rowfirst = settings["rowfirst"] || false
    @images = settings["images"]
    @directory_per_position = settings["directory_per_position"] || false
    @capture_times = settings["capture_times"] ? settings["capture_times"].flatten : nil
    @capture_time_parser = settings["capture_time_parser"] ? File.expand_path(settings["capture_time_parser"]) : "#{File.dirname(__FILE__)}/extract_gigapan_capturetimes.rb"
    @capture_time_parser_inputs = settings["capture_time_parser_inputs"] || "#{@parent.store}/0200-tiles"
    initialize_frames
  end

  def find_directories
    Filesystem.cached_exists?("#{@parent.store}/0100-unstitched") or raise "Could not find #{@parent.store}/0100-unstitched"
    dirs = Dir.glob("#{@parent.store}/0100-unstitched/*").sort.select {|dir| File.directory? dir}
    dirs.empty? and raise 'Found no directories in 0100-unstitched'
    dirs
  end

  def find_images_dpp
    directories = find_directories
    if @cols * @rows != directories.size
      raise "Found #{directories.size} directories in #{@parent.store}/0100-unstitched, but expected #{@cols}x#{@rows}=#{@cols*@rows}"
    end
    dpp_images = []

    directories.each do |dir|
      dpp_images << find_images_in_directory(dir)
      if dpp_images[0].size != dpp_images[-1].size
        raise "Directory #{directories[0]} has #{dpp_images[0].size} images, but directory #{dir} has #{dpp_images[-1].size} images"
      end
    end

    dpp_images[0].size.times.map do |i|
      dpp_images.map {|images| images[i]}
    end
  end

  def find_images
    directories = find_directories
    @framenames = directories.map {|dir| File.basename(dir)}
    directories.map do |dir|
      images = find_images_in_directory(dir)
      images.size == @rows * @cols or raise "Directory #{dir} has #{images.size} images, but expected #{@cols}x#{@rows}=#{@cols*@rows}"
      images
    end
  end

  def initialize_frames
    if @images
      @images.size > 0 or raise "'images' is an empty list"
    elsif @directory_per_position
      @images = find_images_dpp
    else
      @images = find_images
    end

    @images.each_with_index do |frame, i|
      if @cols * @rows != frame.size
        raise "Found #{frame.size} images in 'images' index #{i}, but expected #{@cols}x#{@rows}=#{@cols*@rows}"
      end
    end

    @images.map! {|frame| frame.map! {|image| File.expand_path(image, @parent.store)} }

    @framenames ||= @images.size.times.map {|i| '%06d' % i}

    @tilesize = 256
    @tileformat = "jpg"

    # Read in .gigapan if it exists and we actually need the dimensions from it
    # 1x1 are the dummy dimensions we feed the json file
    first_gigapan = "#{@parent.store}/0200-tiles/#{framenames[0]}.gigapan"
    if @width == 1 && @height == 1 && File.exist?(first_gigapan)
      data = XmlSimple.xml_in(first_gigapan)
      if data
        notes = data["notes"]
        if notes
          dimensions = notes[0].scan(/\d* x \d*/)
          if dimensions
            dimensions_array = dimensions[0].split(" x ")
            if dimensions_array.size == 2
              @width = dimensions_array[0].to_i
              @height = dimensions_array[1].to_i
            end
          end
        end
      end
    end

    @width /= @subsample
    @height /= @subsample
  end

  class Copy
    attr_reader :target

    def initialize(target)
      @target = target
    end
  end

  def copy(x)
    return Copy.new(x)
  end

  def tiles_rules
    targetsets = []
    align_to_eval = nil
    @framenames.size.times do |i|
      framename = @framenames[i]
      align_to = []
      copy_master_geometry_exactly = false
      cmd = stitch_cmd
      unless @master_gigapan
        proc {
          # Change level only inside this proc
          # Security error will be thrown if a user
          # attempts to pass in system calls, etc
          $SAFE = 4
          align_to_eval = eval(@align_to)
        }.call
        if align_to_eval.class == Copy
          if align_to_eval.target != i
            align_to += targetsets[align_to_eval.target]
            copy_master_geometry_exactly = true
          end
        else
          align_to_eval.each do |align_to_index|
            if align_to_index >= i
              raise "align_to for index #{i} yields #{align_to_index}, which >= #{i}"
            end
            if align_to_index >= 0
              align_to += targetsets[align_to_index]
            end
          end
          if align_to == [] && i > 0
            raise "align_to is empty for index #{i} but can only be empty for index 0"
          end
        end
      else
        align_to << @master_gigapan
        copy_master_geometry_exactly = true
        data = XmlSimple.xml_in(@master_gigapan)
        if data
          vignette_curve = data["vignette_curve"]
          if vignette_curve
            c1 = vignette_curve[0]['vector'][0]['elt'][0]
            c2 = vignette_curve[0]['vector'][0]['elt'][1]
            cmd += ["--vignette-curve", c1, c2]
          end
        end
      end
      target_prefix = "#{@parent.store}/0200-tiles/#{framename}"
      if $cache_folder
        cache_prefix = "#{$cache_folder}/#{framename}"
      end
      cmd += @rowfirst ? ["--rowfirst", "--ncols", @cols] : ["--nrows", @rows]
      # Stitch 1.x:
      # stitch_cmd << "--license-key AATG-F89N-XPW4-TUBU"
      # Stitch 2.x:
      cmd += ['--license-key', 'ACTG-F8P9-AJY3-6733']

      cmd += @stitcher_args.split

      if @camera_response_curve
        cmd += ["--load-camera-response-curve", @camera_response_curve]
      end

      suffix = "#{temp_file_unique_fragment}"

      if $cache_folder
        cmd += ["--save-as", "#{cache_prefix}-#{suffix}.gigapan"]
      else
        cmd += ["--save-as", "#{target_prefix}-#{suffix}.gigapan"]
      end
      # Only get files with extensions.  Organizer creates a subdir called "cache",
      # which this pattern will ignore
      images = @images[i]
      cached_images = []
      cache_cmd = ["sed"] + ["-i"]
      if $cache_folder
        cp_cmd = ["cp"]
        images.each do |orig_image|
          temp_string = "#{cache_prefix}-photos/"+File.basename(orig_image)
          cached_images << temp_string
          cp_cmd += ["#{orig_image}"]
          cache_cmd += ["-e"] + ["'s/#{temp_string.gsub('/','\/')}/#{orig_image.gsub('/','\/')}/g'"]
        end
        cache_cmd += ["#{cache_prefix}-#{suffix}.gigapan"]
        cp_cmd += ["#{cache_prefix}-photos"]
      end

      if images.size != @rows * @cols
        raise "There should be #{@rows}x#{@cols}=#{@rows*@cols} images for frame #{i}, but in fact there are #{images.size}"
      end

      stitcher_input_image_list = "#{target_prefix}-input-image-list-#{suffix}"

      unless File.exists?("#{target_prefix}.gigapan")
        FileUtils.mkdir_p(File.dirname(stitcher_input_image_list))
        File.open(stitcher_input_image_list, 'w') do |file|
          if $cache_folder
            file.write(cached_images.join("\n"))
          else
            file.write(images.join("\n"))
          end
        end
      end

      cmd += ["--image-list", stitcher_input_image_list]

      if copy_master_geometry_exactly
        cmd << "--copy-master-geometry-exactly"
      end
      align_to.each do |master|
        cmd += ["--master", master]
      end
      cmd << "--stitch-quit"

      if $cache_folder
        targetsets << Rule.add("#{target_prefix}.gigapan",
                             align_to,
                             [
                              ['mkdir', '-p', "#{cache_prefix}-photos"],
                              cp_cmd,
                              cmd,
                              cache_cmd,
                              ['rm', '-rf', "#{target_prefix}.gigapan"],
                              ['rm', '-rf', "#{target_prefix}.data"],
                              ['mv', "#{cache_prefix}-#{suffix}.gigapan", "#{target_prefix}.gigapan"],
                              ['mv', "#{cache_prefix}-#{suffix}.data", "#{target_prefix}.data"],
                              ['rm', '-rf', "#{cache_prefix}-photos"],
                              ['rm', stitcher_input_image_list]
                             ])
      else
        targetsets << Rule.add("#{target_prefix}.gigapan",
                             align_to,
                             [
                              cmd,
                              ['mv', "#{target_prefix}-#{suffix}.gigapan", "#{target_prefix}.gigapan"],
                              ['mv', "#{target_prefix}-#{suffix}.data", "#{target_prefix}.data"],
                              ['rm', stitcher_input_image_list]
                             ])
      end
    end
    targetsets.flatten(1)
  end
end

$sourcetypes['stitch'] = StitchSource

class Compiler
  attr_reader :source, :tiles_dir, :raw_tilestack_dir, :tilestack_dir, :videosets_dir, :urls, :store, :video_filter
  attr_reader :id, :versioned_id

  def to_s
    "#<Compiler name=#{name} width=#{@width} height=#{@height}>"
  end

  def initialize(settings)
    @urls = settings["urls"] || {}
    @id = settings["id"] #|| raise("Time Machine must have unique ID")
    # @version = settings["version"] || raise("Time Machine must have version")
    # @versioned_id = "#{@id}-v#{@version}"
    # @label = settings["label"] || raise("Time Machine must have label")
    # STDERR.puts "Timemachine #{@id} (#{@label})"
    @store = settings['store'] || raise("No store")
    @store = File.expand_path(@store)
    STDERR.puts "Store is #{@store}"
    @stack_filter = settings['stack_filter']
    @video_filter = settings['video_filter']
    @tiles_dir = "#{store}/0200-tiles"
    @tilestack_dir = "#{store}/0300-tilestacks"
    @raw_tilestack_dir = @stack_filter ? "#{store}/0250-raw-tilestacks" : @tilestack_dir
    source_info = settings["source"] || raise("Time Machine must have source")
    initialize_source(source_info)
    if @source.respond_to? :tilestack_root_path
      @raw_tilestack_dir = @source.tilestack_root_path
    end

    # remove any old tmp json files which are passed to tilestacktool
    Filesystem.rm("#{@tiles_dir}/tiles-*.json")

    initialize_tilestack

    destination_info = settings["destination"]
    if ! destination_info
      destination_info = {"type" => "local"}
      STDERR.puts "No destination specified;  defaulting to #{JSON.pretty_generate(destination_info)}"
    end
    initialize_destination(destination_info)

    STDERR.puts "#{(@source.framenames.size*@source.width*@source.height/1e+9).round(1)} gigapixels total video content"
    STDERR.puts "#{@source.framenames.size} frames"
    STDERR.puts "#{(@source.width*@source.height/1e6).round(1)} megapixels per frame (#{@source.width}px x #{@source.height}px)"

    #initialize_original_images
    @videoset_compilers = settings["videosets"].map {|json| VideosetCompiler.new(self, json)}

    if $os == 'windows'
      mount_point = Sys::Filesystem.mount_point(@videosets_dir)
      stat = Sys::Filesystem.stat(mount_point)
      mb_available = (stat.block_size * stat.blocks_free / 1024 / 1024).floor
    else
      mb_available = (`df -m /`.split(/\b/)[24].to_i).floor
    end

    # TODO
    # Very rough estimates. Hard to get right though because the bitrates are not the same
    # Also need to take into account intermediate steps of tilestacks
    crf_bits_per_pixel = {
      24 => 24.0,
      26 => 8.0,
      28 => 5.0,
      30 => 4.0,
    }

    # TODO
    # Any crf < 24 or crf > 30 will have an incorrect size estimate
    bits_per_pixel = crf_bits_per_pixel[settings["videosets"].first["compression"]] || 24

    estimated_output_size = bits_per_pixel * 0.125 * (@source.framenames.size * @source.width * @source.height / 1e+9).round(1) * 1000.0
    STDERR.puts "Estimated final space required: #{estimated_output_size} MB"

    initialize_tiles

    # add time machine viewer templates to the ajax_includes file
    include_ajax_file("templates/player_template.html")
    include_ajax_file("templates/time_warp_composer.html")
    include_ajax_file("templates/browser_not_supported_template.html")
    include_ajax_file("templates/annotation_editor.html")

  end

  def initialize_source(source_info)
    $sourcetypes[source_info["type"]] or raise "Source type must be one of #{$sourcetypes.keys.join(" ")}"
    @source = $sourcetypes[source_info["type"]].new(self, source_info)
    # Filesystem.write_file('autogenerated_framenames.txt', @source.framenames.join("\n"))
  end

  def initialize_tilestack
    Filesystem.mkdir_p @raw_tilestack_dir
    Filesystem.mkdir_p @tilestack_dir
    r = {'width' => @source.width, 'height' => @source.height,
         'tile_width' => @source.tilesize, 'tile_height' => @source.tilesize,
         'frame_count' => @source.framenames.size }
    Filesystem.write_file("#{@tilestack_dir}/r.json", JSON.pretty_generate(r))
  end

  def initialize_destination(destination_info)
    if destination_info.class == String
      @videosets_dir = destination_info
    else
      @videosets_parent_dir="#{store}/0400-videosets"
      @videosets_dir="#{@videosets_parent_dir}/#{@versioned_id}"
      if ! Filesystem.cached_exists? @videosets_parent_dir
        case destination_info["type"]
        when "local"
          Filesystem.mkdir_p @videosets_parent_dir
        when "symlink"
          target = destination_info["target"] or raise("Must specify 'target' for 'symlink' in 'destination'")
          Filesystem.cached_exists? target or raise("'target' path #{target} does not exist")
          File.symlink(target, @videosets_parent_dir)
        else
          raise "Destination type not recognized"
        end
      end
    end
    if not Filesystem.cached_exists? @videosets_dir
      videosets_tmp = "#{@videosets_dir}.#{temp_file_unique_fragment}"
      Filesystem.mkdir_p videosets_tmp
      Filesystem.cp_r ['css', 'images', 'js', 'templates', 'update_ajax_includes.rb', 'template_includes.js', 'VERSION'].map{|path|"#{$explorer_source_dir}/#{path}"}, videosets_tmp
      Filesystem.cp "#{$explorer_source_dir}/public/tmca_viewer.html", "#{videosets_tmp}/view.html"
      Filesystem.mv videosets_tmp, @videosets_dir
    end
  end

  def initialize_tiles
    tileset = Tileset.new(@source.width, @source.height, @source.tilesize)
    @all_tiles = Set.new tileset.enumerate
    @max_level = tileset.max_level
    @base_tiles = tileset.enumerate_max_level
    @subsampled_tiles = tileset.enumerate_subsampled_levels
  end

  def all_tiles_rule
    Rule.touch("#{@tiles_dir}/COMPLETE", @source.tiles_rules)
  end

  def raw_tilestack_path(tile)
    "#{@raw_tilestack_dir}/#{tile.path}.ts2"
  end

  def tilestack_path(tile)
    "#{@tilestack_dir}/#{tile.path}.ts2"
  end

  def tilestack_rule(tile, dependencies)
    if Rule.has_target?(tile)
      return dependencies
    end
    children = tile.children.select {|t| @all_tiles.member? t}
    if children.size > 0
      subsampled_tilestack_rule(tile, children, dependencies)
    else
      base_tilestack_rule(tile, dependencies)
    end
  end

  def metadata_tilestack
    return Tile.new(0, 0, @max_level)
  end

  def subsampled_tilestack_rule(target_idx, children, dependencies)
    target = tilestack_path(target_idx)
    rule_dependencies = children.flat_map {|child| tilestack_rule(child, dependencies)}
    rule_dependencies << tilestack_path(metadata_tilestack)

    cmd = tilestacktool_cmd
    cmd << "--create-parent-directories"
    frames = {'frames' => {'start' => 0, 'end' => @source.framenames.size - 1},
              'bounds' => target_idx.bounds(@source.tilesize, @max_level)}
    cmd += ['--path2stack-downsize', @source.tilesize, @source.tilesize, JSON.generate(frames), @tilestack_dir]
    cmd += ['--save', target]
    Rule.add(target, rule_dependencies, [cmd])
  end

  def base_tilestack_rule(target_idx, dependencies)
    targets = raw_base_tilestack_rule(target_idx, dependencies)
    if @stack_filter
      src = raw_tilestack_path(target_idx)
      dest = tilestack_path(target_idx)
      cmd = tilestacktool_cmd
      cmd << "--create-parent-directories"
      cmd += ['--load', src]
      cmd += @stack_filter
      cmd += ['--save', dest]
      targets = Rule.add(dest, targets, [cmd])
    end
    targets
  end

  def raw_base_tilestack_rule(target, dependencies)
    if @source.respond_to?(:tilestack_rules)
      # raw tilestacks are produced by the source directly
      return dependencies
    end

    # Build tilestacks from source tiles
    inputs = @source.framenames.map {|frame| "#{@tiles_dir}/#{frame}.data/tiles/#{target.path}.#{@source.tileformat}"}

    Filesystem.mkdir_p @tiles_dir
    json = {"tiles" => inputs}
    path = "#{@tiles_dir}/tiles-#{Process.pid}-#{Time.now.to_f}-#{rand(101010)}.json"
    Filesystem.write_file(path, JSON.fast_generate(json))

    target = raw_tilestack_path(target)
    cmd = tilestacktool_cmd
    cmd << "--create-parent-directories"
    cmd << "--delete-source-tiles" if !$preserve_source_tiles
    cmd += ["--loadtiles-from-json", path]
    cmd += ["--save", target]
    Rule.add(target, dependencies, [cmd, ["rm", path]])
  end

  # How are tilstacks created?
  #
  # Tilestacks are created in a quadtree, with the root tilestack at level 0, depending on 4 level 1 tilestacks, all the
  # way down to the highest level max_level, which is the base of the tree
  #
  # levels = 0 (root) down to max_level-1:
  #    Stored in @tilestacks_dir (0300-tilestacks)
  #    Computed by subsampled_tilestack_rule from up to 4 children each
  #
  # max_level (base level):
  #    These are computed in one of several ways, depending on whether your image
  #    source can natively produce tilestacks, and whether you have a stack filter
  #    to apply
  #
  #    base tilestack (not raw):
  #         Stored in @tilestacks_dir (0300-tilestacks)
  #         A base-level tilestack, created by applying the stack filter (@stack_filter) to
  #         the corresponding raw base tilestack.  If no stack filter exists, the base tilestack
  #         is simply the raw base tilestack (no copy is performed, but rather @raw_tilestack_dir
  #         is set to 0300-tilestacks instead of 0250-raw-tilestacks)
  #
  #    raw base tilestack:
  #         Stored in @raw_tilestack_dir (0250-raw-tilestacks or 0300-tilestacks)
  #         A base-level tilestack, created by tilestacktool directly from image source tiles,
  #         or, if supported by the image source, created directly by the image source.

  def all_tilestacks_rule
    if $compute_tilestacks.none?
      STDERR.puts "   Skipping tilestack creation"
      []
    else
      STDERR.puts "   #{@base_tiles.size} base tiles per input frame (#{@source.tilesize}x#{@source.tilesize} px)"
      if @source.respond_to?(:tilestack_rules)
        # The source natively supplies tilestacks.  We should depend on these
        targets = @source.tilestack_rules
        targets = Rule.touch("#{@raw_tilestack_dir}/SOURCE_COMPLETE", targets)
      else
        # The source supplies tiles.  We should depend on these
        targets = all_tiles_rule
      end
      targets = tilestack_rule(Tile.new(0,0,0), targets)
      Rule.touch("#{@tilestack_dir}/COMPLETE", targets)
    end
  end

  # def tilestack_cleanup_rule
  #   #remove remaining .jpgs leftover and all sub directories
  #   #leave the top-most directory (*.data/tiles/r.info)
  #   #do not do anything if subsmaple is not 1 for the tiling
  #   cmd = @@global_parent.source.subsample == 1 ? ["find #{@tiles_dir} -name *.jpg | xargs rm -f",
  #                                                  "find #{@tiles_dir} -type d | xargs rmdir --ignore-fail-on-non-empty"] : ["echo"]
  #   dependencies = all_tilestacks_rule.targets
  #   Rule.new(["tilestack-cleanup"], dependencies,
  #            cmd,
  #            {:local=>true})
  # end

  def videoset_rules
    # dependencies=tilestack_cleanup_rule.targets
    dependencies = all_tilestacks_rule
    Rule.touch("#{@videosets_dir}/COMPLETE", @videoset_compilers.flat_map {|vc| vc.rules(dependencies)})
  end

  # def capture_times_rule
  #   cmd = ["ruby #{@@global_parent.source.capture_time_parser}",
  #          "#{@@global_parent.source.capture_time_parser_inputs}",
  #          "#{@videosets_dir}/tm.json"]
  #   Rule.add("capture_times", videoset_rules, [cmd])
  # end

  def capture_times_rule
    source = @@global_parent.source.capture_times.nil? ? @@global_parent.source.capture_time_parser_inputs : $jsonfile
    ruby_path = ($os == 'windows') ? File.join(File.dirname(__FILE__), "/../ruby/windows/bin/ruby.exe") : "ruby"
    extra = (defined?(@@global_parent.source.capture_time_print_milliseconds) and @@global_parent.source.capture_time_print_milliseconds) ? "--print-milliseconds" : ""
    Rule.add("capture_times", videoset_rules, [[ruby_path, @@global_parent.source.capture_time_parser, source, "#{@videosets_dir}/tm.json", "-subsample-input", @@global_parent.source.subsample_input, extra]])
  end

  def compute_rules
    @@global_parent.source.capture_time_parser && Filesystem.cached_exists?(@@global_parent.source.capture_time_parser) ? capture_times_rule : videoset_rules
  end

  def info
    ret = {}
    ret = {
      'datasets' => @videoset_compilers.map do |vc|
        {
          'id' => vc.id,
          'name' => vc.label
        }
      end,
      'sizes' => @videoset_compilers.map(&:label)
    }
    @versioned_id and ret['base-id'] = @versioned_id
    @label and ret['name'] = @label
    @id and ret['id'] = @id
    ret
  end

  def write_json
    write_json_and_js("#{@videosets_dir}/tm", 'org.cmucreatelab.loadTimeMachine', info)
    include_ajax "./tm.json", info
    @videoset_compilers.each { |vc| vc.write_json }
  end

  def write_rules
    grouped_rules = Rule.all

    #STDERR.puts "Combined #{Rule.all.size-grouped_rules.size} rules to #{grouped_rules.size}"

    all_targets = grouped_rules.flat_map(&:targets)

    rules = []
    rules << "# generated by ct.rb"
    rules << "all: #{all_targets.join(" ")}"
    rules += grouped_rules.map { |rule| rule.to_make }
    Filesystem.write_file("#{store}/rules.txt", rules.join("\n\n"))
  end
end


# Rule completed:
#    Label rule :done
#    For each target
#       Record that target now exists
#       Loop over @rules_waiting_for_dependency[target] and
#          remove target from @dependencies_holding_up_rule[rule]
#          if @dependencies_holding_up_rule[rule] is empty, and rule is :waiting, label rule :ready
#

class Maker
  def initialize(rules)
    @ndone = 0
    @rules = rules
    @ready = []
    @dependencies_holding_up_rule = {}
    @rules_waiting_for_dependency = {}

    # Does target exist?  true or false
    @targets={}

    # Status of each rule.  :waiting, :ready, :executing, :done
    @status = {}

    STDERR.write "Checking targets from #{rules.size} rules...  0%"
    rules.each_with_index do |rule, i|
      if i*100/rules.size > (i-1)*100/rules.size
        STDERR.write("\b\b\b\b%3d%%" % (i*100/rules.size))
      end
      compute_rule_status(rule)
    end
    STDERR.write("\b\b\b\bdone.\n")
  end

  #
  # Compute rule status (done once per rule, at beginning)
  #    If every target exists, label :done
  #    If all dependencies exist, label rule :ready.  Return
  #    Label rule :waiting
  #    For each dependency that doesn't exist:
  #      add rule to @rules_waiting_for_dependency[dep]
  #      add dep to @dependencies_holding_up_rule[rule]
  #
  def compute_rule_status(rule)
    @dependencies_holding_up_rule[rule] = Set.new
    if rule.targets.all? { |target| target_exists? target }
      # If every target exists, label :done
      @status[rule] == :done
      @ndone += 1
    else
      rule.dependencies.each do |dep|
        if not target_exists? dep
          # For dependencies which don't exist, set up cross references
          (@rules_waiting_for_dependency[dep] ||= Set.new) << rule
          @dependencies_holding_up_rule[rule] << dep
        end
      end
      if @dependencies_holding_up_rule[rule].empty?
        @status[rule] =  :ready
        @ready << rule
      else
        @status[rule] = :waiting
      end
    end
  end

  def target_exists?(target)
    if @targets.member?(target)
      @targets[target]
    elsif File.basename(target) =~ /^dry-run-fake/
      @targets[target] = true
    else
      @targets[target] = Filesystem.cached_exists?(target)
    end
  end

  def rule_completed(rule)
    @status[rule] != :executing and raise("Completed a rule that wasn't executing")
    @ndone += 1
    @status[rule] = :done
    rule.targets.each { |target| create_target target }
  end

  def create_target(target)
    if not @targets[target]
      @targets[target] = true
      (@rules_waiting_for_dependency[target] || []).each do |rule|
        @dependencies_holding_up_rule[rule].delete target
        if @dependencies_holding_up_rule[rule].empty? and @status[rule] == :waiting
          @status[rule] = :ready
          @ready << rule
        end
      end
    end
  end

  def execute_rules(job_no, rules, response)
    begin
      counter = 1

      $check_mem and CheckMem.logvm "Constructing job #{job_no}"

      if !$dry_run && (!$run_remotely || rules.all?(&:local))
        result = rules.flat_map(&:commands).all? do |command|
          STDERR.write "#{date} Job #{job_no} executing #{command.join(' ')}\n"
          if (command[0] == 'rm')
            File.delete(command[1])
          elsif (command[0] == 'mv')
            File.rename(command[1], command[2])
          else
            ret = Kernel.system(*command) # This seems to randomly raise an exception in ruby 1.9
            unless ret
              STDERR.write "Command #{command.join(' ')} failed with ret=#{ret}, #{$?}\n"
            end
            ret
          end
        end
      else
        if $run_remotely_json
          json_file = "job_#{$name}_#{Process.pid}_#{"%04d" % job_no}_#{rules.size}.json"
          $run_remotely_json_dir and json_file = $run_remotely_json_dir + "/" + json_file
          STDERR.write "#{date} Writing #{rules.size} rules to #{json_file}\n"
          open(json_file + ".tmp", "w") do |json|
            json.puts "["
            rules.map(&:commands).each_with_index do |command, i|
              (i % 50000 == 0) and STDERR.write "#{date} Progress: #{i} rules of #{rules.size}\n"
              i > 0 and json.puts ","
              json.write JSON.fast_generate command
            end
            json.puts "\n]"
          end
          File.rename(json_file + ".tmp", json_file)
          command = "#{$run_remotely} --jobs @#{json_file}"
        else
          commands = rules.flat_map(&:commands)
          command = commands.join(" && ")
          command = "#{$run_remotely} '#{command}'"
        end
        STDERR.write "#{date} Job #{job_no} #{$dry_run ? "would execute" : "executing"} #{command}\n"

        if $dry_run
          result = true
        else
          # Retry up to 3 times if we fail
          while (!(result = system(command)) && counter < 4) do
            STDERR.write "#{date} Job #{job_no} failed, retrying, attempt #{counter}\n"
            counter += 1
          end
        end
      end

      if !result
        @failed_jobs << job_no
        STDERR.write "#{date} Job #{job_no} failed too many times; aborting\n"
        @aborting = true
        response.push([job_no, [], Thread.current])
      else
        STDERR.write "#{date} Job #{job_no} completed successfully\n"
        response.push([job_no, rules, Thread.current])
      end
    rescue Exception => e
      STDERR.write "#{date} Job #{job_no} failed to execute.\n"
      STDERR.write "#{e.message}\n"
      STDERR.write "#{e.backtrace.inspect}\n"
      @aborting = true
      response.push([job_no, [], Thread.current])
    end
  end

  # Returns true if success, false if failed or need to retry (e.g. now have first image dimensions)

  def make(max_jobs, max_rules_per_job)
    begin_time = Time.now
    # build all rules
    # rule depends on dependencies:
    #    some dependencies already exist
    #    other dependencies are buildable by other rules
    # 1
    # loop through all rules
    #    if target already exists in filesystem, the rule is done
    #    if dependencies already exist in filesystem, the rule is ready to run
    #    for dependencies that don't already exist, create mapping from dependency to rule
    # 2
    # execute ready to run rules
    # when rule is complete, loop over rule targets and re-check any rules that depended on these targets to see
    #   if the rules can be moved to the "ready to run" set

    jobs_executing = 0
    rules_executing = 0
    response = Queue.new

    @failed_jobs = []
    @aborting = false
    current_job_no = 0
    initial_ndone = @ndone

    print_status(rules_executing, nil)

    while @ndone < @rules.size
      # Restart the process if we are stitching from source and we have not gotten dimensions from the first gigapan
      if @@global_parent.source.class.to_s == "StitchSource" && @ndone > 0 && @@global_parent.source.width == 1 && @@global_parent.source.height == 1
        STDERR.write "Initial .gigapan file created. We now have enough info to get the dimensions.\n"
        STDERR.write "Clearing all old rules...\n"
        Rule.clear_all
        STDERR.write "Restarting process with new dimensions...\n"
        # Return false will cause a retry
        return false
      end
      # Send jobs
      if !@aborting && !@ready.empty?
        jobs_to_fill = max_jobs - jobs_executing
        rules_per_job = (@ready.size.to_f / jobs_to_fill).ceil
        rules_per_job = [rules_per_job, max_rules_per_job].min
        rules_per_job > 0 or raise("assert")
        while !@ready.empty? && jobs_executing < max_jobs
          1.times do
            job_no = (current_job_no += 1)
            # Make sure to_run is inside this closure
            to_run = @ready.shift(rules_per_job)
            to_run.size > 0 or raise("assert")
            to_run.each do |rule|
              @status[rule] == :ready or raise("assert")
              @status[rule]=:executing
            end
            STDERR.write "#{date} Job #{job_no} executing #{to_run.size} rules #{to_run[0]}...\n"
            Thread.new { execute_rules(job_no, to_run, response) }
            jobs_executing += 1
            rules_executing += to_run.size
          end
        end
      end
      # Wait for job to finish
      if @aborting && jobs_executing == 0
        STDERR.write "#{date}: Aborting because of failed job(s) [#{@failed_jobs.sort.join(', ')}]\n"
        exit 1
      end
      if @ready.empty? && jobs_executing == 0
        STDERR.write "#{date}: Don't know how to build targets, aborting\n"
        exit 1
      end
      (job_no, rules, thread) = response.pop
      $dry_run || thread.join
      jobs_executing -= 1
      rules_executing -= rules.size
      #STDERR.write "#{date}: Completing #{rules.size} rules...\n"
      rules.each {|rule| rule_completed(rule)}
      #STDERR.write "#{date}: Done\n"
      print_status(rules_executing, jobs_executing)
    end
    duration = Time.now.to_i - begin_time.to_i
    STDERR.write "#{date}: Completed #{@rules.size-initial_ndone} rules (#{current_job_no} jobs) in #{duration/86400}d #{duration%86400/3600}h #{duration%3600/60}m #{duration%60}s\n"
    if @aborting
      STDERR.write "Aborted due to error"
      return false
    else
      return true
    end
  end

  def print_status(rules, jobs)
    status = []
    status << "#{(@ndone*100.0/@rules.size).round(1)}% rules finished (#{@ndone}/#{@rules.size})"
    rules && jobs && status << "#{rules} rules executing (in #{jobs} jobs)"
    status << "#{@ready.size} rules ready to execute"
    status << "#{@rules.size-@ndone-@ready.size-rules} rules awaiting dependencies"
    STDERR.write "#{date} Rules #{status.join(". ")}\n"
  end

end

retry_attempts = 3
destination = nil

exe_suffix = ($os == 'windows') ? '.exe' : ''

tilestacktool_search_path = [File.expand_path("#{File.dirname(__FILE__)}/tilestacktool#{exe_suffix}"),
             File.expand_path("#{File.dirname(__FILE__)}/../tilestacktool/tilestacktool#{exe_suffix}")]



# If tilestacktool is not found in the project directory, assume it is in the user PATH
$tilestacktool = tilestacktool_search_path.find {|x| File.exists?(x)}

if $tilestacktool
  STDERR.puts "Found tilestacktool at #{$tilestacktool}"
else
  STDERR.puts "Could not find tilestacktool in path #{tilestacktool_search_path.join(':')}"
end

stitchpath = nil

def stitch_version_from_path(path)
  path.scan(/(GigaPan |gigapan-)(\d+)\.(\d+)\.(\d+)/) do |a,b,c,d|
    return sprintf('%05d.%05d.%05d', b.to_i, c.to_i, d.to_i)
  end
  return ''
end

# Check for stitch in the registry
if $os == 'windows'
  KEY_32_IN_64 = 'Software\Wow6432Node\Microsoft\Windows\CurrentVersion\Installer\Folders'
  KEY = 'Software\Microsoft\Windows\CurrentVersion\Installer\Folders'
  KEY_WOW64_64KEY = 0x0100

  reg = read_from_registry(Win32::Registry::HKEY_LOCAL_MACHINE,KEY_32_IN_64)
  reg = read_from_registry(Win32::Registry::HKEY_LOCAL_MACHINE,KEY) if reg.nil?

  if reg
    reg.each do |name, type, data|
      if stitch_version_from_path(name) > '00002' # Look for at least version 2.x
        stitchpath = name
        break
      end
    end
    reg.close
    if stitchpath.nil?
      STDERR.puts "Could not find an entry for stitch in the registry. Checking default installation path..."
    else
      stitchpath = File.join(stitchpath,"stitch.exe")
      STDERR.puts "Found stitch at: #{stitchpath}"
    end
  else
    STDERR.puts "Could not read registry."
  end
end

# If a path to stitch was not found in the registry or we are not on windows, check the default install path
if stitchpath.nil?
  if $os == 'osx'
    search = '/Applications/GigaPan */GigaPan Stitch */Contents/MacOS/GigaPan Stitch *'
  elsif $os == 'windows'
    search = 'C:/Program*/GigaPan*/*/stitch.exe'
  elsif $os == 'linux'
    search = File.expand_path(File.dirname(__FILE__)) + '/../gigapan-*.*.*-linux/stitch'
  end
  if search
    # If multiple versions, grab the path of the latest release
    found = Dir.glob(search).sort{|a,b| stitch_version_from_path(a) <=> stitch_version_from_path(b)}.reverse[0]
    if not found
      STDERR.puts "Could not find stitch in default installation path: #{search}"
    elsif stitch_version_from_path(found) > '00002' # Look for at least version 2.x
      STDERR.puts "Found stitch at: #{found}"
      stitchpath = found
    else
      STDERR.puts "Found stitch at: #{found} but the version found does not support timelapses. Please update to at least version 2.x"
    end
  end
end

# If stitch is not found or the one found is too old, assume it is in the user PATH
$stitch = (stitchpath && File.exists?(stitchpath)) ? stitchpath : 'stitch'

explorer_source_search_path = [File.expand_path("#{File.dirname(__FILE__)}/timemachine-viewer"),
             File.expand_path("#{File.dirname(__FILE__)}/../timemachine-viewer")]

$explorer_source_dir = explorer_source_search_path.find {|x| File.exists? x}
if $explorer_source_dir
  STDERR.puts "Found explorer sources at #{$explorer_source_dir}"
else
  STDERR.puts "Could not find explorer sources in path #{explorer_source_search_path.join(':')}"
end

$jsonfile = nil
destination = nil

njobs = 1
rules_per_job = 1

cmdline = "ct.rb #{ARGV.join(' ')}"

while !ARGV.empty?
  arg = ARGV.shift
  if arg == '-j'
    njobs = ARGV.shift.to_i
  elsif arg == '-r'
    rules_per_job  = ARGV.shift.to_i
  elsif arg == '--skip-tilestacks'
    $compute_tilestacks = Partial.none
  elsif arg == '--partial-tilestacks'
    $compute_tilestacks = Partial.new(ARGV.shift.to_i, ARGV.shift.to_i)
  elsif arg == '--preserve-source-tiles'
    $preserve_source_tiles = true
  elsif arg == '--skip-videos'
    $compute_videos = Partial.none
  elsif arg == '--partial-videos'
    $compute_videos = Partial.new(ARGV.shift.to_i, ARGV.shift.to_i)
  elsif arg == '-v'
    $verbose = true
  elsif arg == '--remote'
    $run_remotely = ARGV.shift
  elsif arg == '--remote-json'
    $run_remotely_json = true
  elsif arg == '--remote-json-dir'
    $run_remotely_json_dir = ARGV.shift
  elsif arg == '--tilestacktool'
    $tilestacktool = File.expand_path ARGV.shift
  elsif File.extname(arg) == '.tmc'
    $jsonfile = File.basename(arg) == 'definition.tmc' ? arg : "#{arg}/definition.tmc"
  elsif File.extname(arg) == '.timemachine'
    destination = File.expand_path(arg)
  elsif arg == '-n' || arg == '--dry-run'
    $dry_run = true
  elsif arg == '--selftest'
    exit self_test ? 0 : 1
  elsif arg == '--selftest-no-stitch'
    exit self_test(false) ? 0 : 1
  elsif arg == '--cache_folder'
    $cache_folder = File.expand_path ARGV.shift
  elsif arg == "--skip-trailer"
    $skip_trailer = true
  elsif arg == "--skip-leader"
    $skip_leader = true
  elsif arg == "--sort-by-exif-dates"
    $sort_by_exif_dates = true
  else
    STDERR.puts "Unknown arg #{arg}"
    usage
  end
end

if !$jsonfile
  usage "Must give path to description.tmc"
end

if !destination
  usage "Must specify destination.timemachine"
end

# store is where all the intermediate files are created (0[123]00-xxx)
# store must be an absolute path
#
# Normally, the definition file is in a path foo.tmc/definition.tmc.  In this case,
# the store is foo.tmc
#
# The store directory can be overridden in the .tmc file itself, using the "store" field

store = nil

if $jsonfile
  if File.extname(File.dirname($jsonfile)) == ".tmc"
    store = File.dirname($jsonfile)
  end
end

STDERR.puts "Reading #{$jsonfile}"
definition = JSON.parse(Filesystem.read_file($jsonfile))
definition['destination'] = destination
# .tmc "store" field overrides
store = definition['store'] || store

store or raise "No store specified.  Please place your definition.tmc inside a directory name.tmc and that will be your store"

store = File.expand_path store
definition['store'] = store   # For passing to the compiler.  Not ideal

$name = without_extension(File.basename(store))

Filesystem.cache_directory store
Filesystem.cache_directory destination

compiler = nil

retry_attempts.times do
  compiler = Compiler.new(definition)
  compiler.write_json
  Filesystem.write_file("#{@@global_parent.videosets_dir}/ajax_includes.js",
                    ajax_includes_to_javascript)
  STDERR.puts "Wrote initial #{@@global_parent.videosets_dir}/ajax_includes.js"
  compiler.compute_rules # Creates rules, which will be accessible from Rule.all
  $check_mem and CheckMem.logvm "All rules created"
  # compiler.write_rules
  if Maker.new(Rule.all).make(njobs, rules_per_job)
    break
  end
end

# remove any old tmp json files which are passed to tilestacktool
Filesystem.rm("#{@@global_parent.tiles_dir}/tiles-*.json")

# Add tm.json to ajax_includes.js again to include the addition of capture times
include_ajax("./tm.json", JSON.parse(Filesystem.read_file("#{@@global_parent.videosets_dir}/tm.json")))

Filesystem.write_file("#{@@global_parent.videosets_dir}/ajax_includes.js",
                      ajax_includes_to_javascript)
STDERR.puts "Wrote final #{@@global_parent.videosets_dir}/ajax_includes.js"

STDERR.puts "If you're authoring a mediawiki page at timemachine.gigapan.org, you can add this to #{compiler.urls['view'] || "your page"} to see the result: {{TimelapseViewer|timelapse_id=#{compiler.versioned_id}|timelapse_dataset=1|show_timewarp_composer=true}}"
compiler.urls['track'] and STDERR.puts "and update tracking page #{compiler.urls['track']}"

STDERR.puts "View at file://#{destination}/view.html"

end_time = Time.new
STDERR.puts "Execution took #{end_time-start_time} seconds"

if profile
  result = RubyProf.stop
  printer = RubyProf::GraphPrinter.new(result)
  STDERR.puts "Writing profile to #{profile}"
  open(profile, "w") do |out|
    out.puts "Profile #{Time.new}"
    out.puts cmdline
    out.puts "Execution took #{end_time-start_time} seconds"
    out.puts
    printer.print(out, {})
  end
end
