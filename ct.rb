#!/usr/bin/env ruby

# You need Ruby 1.9.x to run this script
# On mac:
# curl -L get.rvm.io | bash -s stable
# source ~/.bash_profile
# rvm requirements
# rvm install 1.9.3 --with-gcc=clang
# cd ~/bin; ln -s ~/.rvm/rubies/ruby-1.9.3-*/bin/ruby .
# which ruby
#
# You need the xml-simple gem to run this script
# [sudo] gem install xml-simple

if RUBY_VERSION < "1.9"
  # Try to run with ruby19
  STDERR.puts __FILE__
  STDERR.puts ENV['PATH']
  exec("ruby19", __FILE__, *ARGV)
end
  
require 'json'
require 'set'
require 'thread'
require 'open-uri'
require File.dirname(__FILE__) + '/image_size'
require File.dirname(__FILE__) + '/tile'
require File.dirname(__FILE__) + '/xmlsimple'
require 'fileutils'

$verbose = false
@@global_parent = nil

# require 'ruby-debug'
# require 'ruby-prof'
# RubyProf.start

#
# DOCUMENTATION IS IN README_CT.TXT
#

def date
  Time.now.strftime("%Y-%m-%d %H:%M:%S")
end

def usage(msg=nil)
  msg and STDERR.puts msg
  STDERR.puts "usage: ct.rb [foo.timemachinedefinition]"
  STDERR.puts "ct.rb [-j N] [-r N] [-v] [-l]"
  STDERR.puts "-j N:  number of parallel jobs to run (default ${njobs})"
  STDERR.puts "-r N:  max number of rules per job (default ${rules_per_job})"
  STDERR.puts "-l:    only run jobs locally (implies -j 1 -r 1)"
  STDERR.puts "-v:    verbose"
  STDERR.puts "--tilestacktool path: full path of tilestacktool"
  exit 1
end

def without_extension(filename)
  filename[0..-File.extname(filename).size-1]
end

class VideoTile
  attr_accessor :c, :r, :level, :x, :y, :subsample
  def initialize(c, r, level, x, y, subsample)
    @c = c
    @r = r
    @level = level
    @x = x
    @y = y
    @subsample = subsample
  end

  def path
    "#{@level}/#{@r}/#{@c}"
  end

  def to_s
    "#<VideoTile path='#{path}' r=#{@r} c=#{@c} level=#{@level} x=#{@x} y=#{@y} subsample=#{@subsample}>"
  end

  def source_bounds(vid_width, vid_height)
    {'xmin' => @x, 'ymin' => @y, 'width' => vid_width * @subsample, 'height' => vid_height * @subsample}
  end
end

class Rule
  attr_reader :targets, :dependencies, :commands, :local
  @@all = []
  
  def self.clear_all
    @@all = []
  end
  
  def self.all
    @@all
  end
  
  def array_of_strings?(a)
    a.class == Array && a.map{|e| e.class == String}.reduce{|a,b| a && b}
  end

  def initialize(targets, dependencies, commands, options={})
    array_of_strings?(targets) or raise "targets must be an array of pathnames"
    array_of_strings?(dependencies) or raise "dependencies must be an array of pathnames"
    array_of_strings?(commands) or raise "commands must be an array of strings"
    @targets = targets
    @dependencies = dependencies
    @commands = commands
    @local = options[:local] || false
    @@all << self
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
  attr_reader :videotype, :vid_width, :vid_height, :overlap_x, :overlap_y, :quality, :label, :fps, :show_frameno, :parent
  attr_reader :id
  @@sizes={
    "large"=>{:vid_size=>[1088,624], :overlap=>[1088/4, 624/4]},
    "small"=>{:vid_size=>[768,432], :overlap=>[768/3, 432/3]}
  }
  @@videotypes={
    "h.264"=>{}
  }
  @@leaders={
  	"crf32" => {"small"=>"360", "large"=>"180"},
  	"crf28" => {"small"=>"160", "large"=>"80"},
  	"crf24" => {"small"=>"60", "large"=>"30"}
  }
  
  def initialize(parent, settings)
    @parent = parent
		@@global_parent = parent
    @label = settings["label"] || raise("Video settings must include label")

    @videotype = settings["type"] || raise("Video settings must include type")
    
    size = settings["size"] || raise("Video settings must include size")
    @@sizes.member?(size) || raise("Video size must be one of [#{@@sizes.keys.join(", ")}]")
    (@vid_width, @vid_height) = @@sizes[size][:vid_size]
    (@overlap_x, @overlap_y) = @@sizes[size][:overlap]

    @quality = settings["quality"] || raise("Video settings must include quality")
    @quality = @quality.to_i
    @quality > 0 || raise("Video quality must be > 0")

    @fps = settings["fps"] || raise("Video settings must include fps")
    @fps = @fps.to_f
    @fps > 0 || raise("Video fps must be > 0")

    @show_frameno = settings["show_frameno"]

    @use_leaders = @parent.source.framenames.size > 30
    if @use_leaders
      @@leaders.member?("crf#{@quality}") || raise("Video quality must be one of [#{@@leaders.keys.join(", ")}]")
      @leader = @@leaders["crf#{@quality}"][size]
    end

    initialize_videotiles

    initialize_id
  end

  def initialize_id
    tokens = ["crf#{@quality}", "#{@fps.round}fps"]
    @leader && tokens << "l#{@leader}"
    tokens << "#{@vid_width}x#{@vid_height}"
    @id = tokens.join('-')
  end

  def initialize_videotiles
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


    @videotiles = []

    levels.each do |level|
      level_rows = 1+((@parent.source.height - level[:input_height]).to_f / (@overlap_y * level[:subsample])).ceil
      level_rows = [1,level_rows].max
      level_cols = 1+((@parent.source.width - level[:input_width]).to_f / (@overlap_x * level[:subsample])).ceil
      level_cols = [1,level_cols].max
      @levelinfo << {"rows" => level_rows, "cols" => level_cols}
      #puts "** level=#{level[:level]} subsample=#{level[:subsample]} #{level_cols}x#{level_rows}=#{level_cols*level_rows} videos input_width=#{level[:input_width]} input_height=#{level[:input_height]}"
      level_rows.times do |r|
        y = r * @overlap_y * level[:subsample]
        level_cols.times do |c|
          x = c * @overlap_x * level[:subsample]
          @videotiles << VideoTile.new(c, r, level[:level], x, y, level[:subsample])
        end
      end
    end
  end
  


#	./tilestacktool --path2stack 200 150 '{"frames":{"start":0, "end":3} ,"bounds":{"xmin":150, "ymin":200, "width":400, "height":300}}' testresults/patp4s/transpose --writevideo testresults/patp4s/testvid.mp4 1 24

  def rules(dependencies)
    STDERR.puts "#{id}: #{@videotiles.size} videos"
    @videotiles.map do |vt|
      target = "#{@parent.videosets_dir}/#{id}/#{vt.path}.mp4"
      cmd = [$tilestacktool]
      cmd << "--create-parent-directories"
   
      cmd << '--path2stack'
      cmd += [@vid_width, @vid_height]
      frames = {'frames' => {'start' => 0, 'end' => @parent.source.framenames.size - 1}, 
                'bounds' => vt.source_bounds(@vid_width, @vid_height)};
      cmd << "'#{JSON.generate(frames)}'"
      cmd << @parent.transpose_dir

      cmd += ['--writevideo', target, @fps, @quality]
      # TODO: leader!
      # @leader && cmd += ["--leader", @leader]
      Rule.new([target], dependencies, [cmd.join(" ")])
    end
  end

#*#  def rules(dependencies)
#*#    STDERR.puts "#{id}: #{@videotiles.size} videos"
#*#    @videotiles.map do |vt|
#*#      cmd = ["make-video.py"]
#*#      @show_frameno && cmd << "--label"
#*#      cmd << @parent.transpose_dir
#*#      cmd += [@parent.source.width, @parent.source.height, 0]                             # gigapan dimensions
#*#      cmd += [0, @parent.source.framenames.size-1]                                         # frames to render
#*#      cmd += [vt.x, vt.y, @vid_width*vt.subsample, @vid_height*vt.subsample] # crop
#*#      cmd += [@vid_width, @vid_height]                                      # output size
#*#      target = "#{@parent.videosets_dir}/#{id}/#{vt.path}"
#*#      cmd += ["--tilesize", @parent.source.tilesize]
#*#      @leader && cmd += ["--leader", @leader]
#*#      case @videotype
#*#      when "h.264"
#*#        target += ".mp4"
#*#        cmd += ["--qtfaststart",
#*#                "--ffmpeg",
#*#                # Input settings
#*#                "-s", "#{@vid_width}x#{@vid_height}",
#*#                "-vcodec", "rawvideo",
#*#                "-f", "rawvideo",
#*#                "-pix_fmt", "rgb24",
#*#                "-r", @fps,
#*#                "-i", "-",
#*#                # Output settings
#*#                "-y",
#*#                "-vcodec", "libx264",
#*#                "-vpre",  "hq",
#*#                "-crf", @quality,
#*#                "-g", 10,
#*#                "-bf", 0,
#*#                "-r", @fps]
#*#      when "webm"
#*#        target += ".webm"
#*#        cmd << "--webm"
#*#      else
#*#        raise "unknown video type #{@videotype}"
#*#      end
#*#      cmd << target
#*#      Rule.new([target], dependencies, [cmd.join(" ")])
#*#    end
#*#  end

  def info
    {
      "level_info"   => @levelinfo,
      "nlevels"      => @levelinfo.size,
      "level_scale"  => 2,
      "frames"       => @parent.source.framenames.size,
      "fps"          => @fps,
      "leader"       => @leader || 0,
      "tile_width"   => @overlap_x,
      "tile_height"  => @overlap_y,
      "video_width"  => @vid_width,
      "video_height" => @vid_height,
      "width"        => parent.source.width,
      "height"       => parent.source.height
    }
  end

  def write_json
    system "mkdir -p #{@parent.videosets_dir}/#{id}"
    dest = "#{@parent.videosets_dir}/#{id}/r.json"
    open(dest,"w") do |fh| 
      fh.puts(JSON.pretty_generate(info))
    end
    STDERR.puts("Wrote #{dest}")
  end
end

class ImagesSource
  attr_reader :ids, :width, :height, :tilesize, :tileformat, :subsample, :raw_width, :raw_height
  attr_reader :capture_times, :capture_time_parser, :capture_time_parser_inputs, :framenames

  @@valid_image_extensions = Set.new [".jpg",".jpeg",".png",".tif",".tiff", ".raw", ".kro"]
  
  def initialize(parent, settings)
    @parent = parent
    @@global_parent = parent
    @image_dir="0100-original-images"
    @raw_width = settings["width"]
    @raw_height = settings["height"]
    @subsample = settings["subsample"] || 1
    @images = settings["images"]
    @capture_times = settings["capture_times"]
    @capture_time_parser = settings["capture_time_parser"] || "/home/rsargent/bin/extract_exif_capturetimes.rb"
    @capture_time_parser_inputs = settings["capture_time_parser_inputs"] || "0100-unstitched/"    
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
    if not @images
      @images = (Dir.glob("#{@image_dir}/*.*")+Dir.glob("#{@image_dir}/*/*.*")).sort.select do |image|
        @@valid_image_extensions.include? File.extname(image).downcase
      end
      @images.empty? && usage("No images specified, and none found in #{@image_dir}")
    end
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
  
#  def framename(image_filename)
#    # TODO: kill this!
#    # image_filename is prefixed with @image_dir;  remove this, translate / to @, and remove file extension
#    without_extension(image_filename[@image_dir.size+1..-1].gsub("/","@"))
#  end
  
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
    cmd = [$tilestacktool, '--tilesize', @tilesize, '--image2tiles', target, @tileformat, image]
    Rule.new([target], [image],
             [cmd.join(' ')])
  end
 
  def tiles_rules
    @images.map {|image| image_to_tiles_rule(image)}
  end

end

class GigapanOrgSource
  attr_reader :ids, :width, :height, :tilesize, :tileformat, :framenames, :subsample
  attr_reader :capture_time_parser, :capture_time_parser_inputs
  
  def initialize(parent, settings)
    @parent = parent
		@@global_parent = parent
    @urls = settings["urls"]
    @ids = @urls.map{|url| id_from_url(url)}
    @subsample = settings["subsample"] || 1
    @capture_time_parser = "/home/rsargent/bin/extract_gigapan_capturetimes.rb"
    @capture_time_parser_inputs = "0200-tiles"
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
    (0...@ids.size).map do |i|
      target = "#{@parent.tiles_dir}/#{framename(i)}.data/tiles"
      Rule.new([target], [], ["mirror-gigapan.rb #{@ids[i]} #{target}"])
    end
  end

  def id_from_url(url)
    url.match(/(\d+)/) {|id| return id[0]}
    raise "Can't find ID in url #{url}"
  end
end    

class PrestitchedSource
  attr_reader :ids, :width, :height, :tilesize, :tileformat, :framenames, :subsample
  attr_reader :capture_time_parser, :capture_time_parser_inputs
  
  def initialize(parent, settings)
    @parent = parent
		@@global_parent = parent
    @subsample = settings["subsample"] || 1
    @capture_time_parser = "/home/rsargent/bin/extract_gigapan_capturetimes.rb"
    @capture_time_parser_inputs = "0200-tiles"
    initialize_frames
  end

  def initialize_frames
    @framenames = Dir.glob("0200-tiles/*.data").map {|dir| File.basename(without_extension(dir))}.sort

    data = XmlSimple.xml_in("0200-tiles/#{framenames[0]}.data/tiles/r.info")
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

class StitchSource
  attr_reader :ids, :width, :height, :tilesize, :tileformat, :framenames, :subsample
  attr_reader :align_to, :stitcher_args, :camera_response_curve, :cols, :rows, :rowfirst
  attr_reader :directory_per_position, :capture_time_parser, :capture_time_parser_inputs
  
  def initialize(parent, settings)
    @parent = parent
		@@global_parent = parent
    @subsample = settings["subsample"] || 1
    @width = settings["width"] || 0
    @height = settings["height"] || 0
    @align_to = settings["align_to"] or raise "Must include align-to"
    settings["align_to_comment"] or raise "Must include align-to-comment"
    @stitcher_args = settings["stitcher_args"] or raise "Must include align-to-comment"
    @camera_response_curve = settings["camera_response_curve"] || false
    if @camera_response_curve && !File.exists?(@camera_response_curve)
      raise "Camera response curve set to #{@camera_response_curve} but the file doesn't exist"
    end
    @cols = settings["cols"] or raise "Must include cols"
    @rows = settings["rows"] or raise "Must include rows"
    @rowfirst = settings["rowfirst"] || false
    @directory_per_position = settings["directory_per_position"] || false
    @capture_time_parser = "/home/rsargent/bin/extract_gigapan_capturetimes.rb"
    @capture_time_parser_inputs = "0200-tiles"
    initialize_frames
  end

  def initialize_frames
    if @directory_per_position
      @directories = Dir.glob("0100-unstitched/*").sort
      if @cols * @rows != @directories.size
        raise "Found #{@directories.size} directories but expected #{@cols}x#{@rows}=#{@cols*@rows}"
      end
      @framenames = []
      @dpp_images = []

      @directories.each do |directory|
        @dpp_images << Dir.glob("#{directory}/*.*").sort{|a,b| File.mtime(a) <=> File.mtime(b)}
        if @dpp_images[0].size != @dpp_images[-1].size
          raise "Directories don't all have same number of images"
        end
      end
      
      @dpp_images[0].size.times { |i| @framenames << sprintf("%06d", i) }
    else 
      @framenames = Dir.glob("0100-unstitched/*").map {|dir| File.basename(dir)}.sort
    end
    @tilesize = 256
    @tileformat = "jpg"
    
    # Read in .gigapan if it exists and we actually need the dimensions from it
    # 1x1 are the dummy dimensions we feed the json file
    first_gigapan = "0200-tiles/#{framenames[0]}.gigapan"
    if @width == 1 && @height == 1 && File.exist?(first_gigapan)
      data = XmlSimple.xml_in(first_gigapan)
      unless data.nil?
        notes = data["notes"]
        unless notes.nil?
          dimensions = notes[0].scan(/\d* x \d*/)
          unless dimensions.nil?
            dimensions_array = dimensions[0].split(" x ")
            unless dimensions_array.size != 2
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
    rules = []
    @framenames.each_with_index do |framename, i|
      align_to = []
      copy_master_geometry_exactly = false
      align_to_eval = eval(@align_to)

      if align_to_eval.class == Copy
        if align_to_eval.target != i
          align_to += rules[align_to_eval.target].targets
          copy_master_geometry_exactly = true
        end
      else
        align_to_eval.each do |align_to_index|
          if align_to_index >= i
            raise "align_to for index #{i} yields #{align_to_index}, which >= #{i}"
          end
          if align_to_index >= 0
            align_to += rules[align_to_index].targets
          end
        end
        if align_to == [] && i > 0
          raise "align_to is empty for index #{i} but can only be empty for index 0"
        end
      end
      source_dir = "0100-unstitched/#{framename}"
      target_prefix = "0200-tiles/#{framename}"
      stitch_cmd = ["stitch"]
      stitch_cmd += @rowfirst ? ["--rowfirst", "--ncols", @cols] : ["--nrows", @rows]
      # Stitch 1.x:
      # stitch_cmd << "--license-key AATG-F89N-XPW4-TUBU"
      # Stitch 2.x:
      stitch_cmd << "--license-key ACTG-F8P9-AJY3-6733"
      
      stitch_cmd += @stitcher_args.split
      if @camera_response_curve
        stitch_cmd += ["--load-camera-response-curve", @camera_response_curve]
      end

      suffix = "tmp-#{Time.new.to_i}"

      stitch_cmd += ["--save-as", "#{target_prefix}-#{suffix}.gigapan"]
      # Only get files with extensions.  Organizer creates a subdir called "cache",
      # which this pattern will ignore
      if @directory_per_position
        images = @dpp_images.map{|list| list[i]}
      else
        images = Dir.glob("#{source_dir}/*.*")
        
        if images.size != @rows * @cols
          raise "There should be #{@rows}x#{@cols}=#{@rows*@cols} images in #{source_dir}, but in fact there are #{images.size}"
        end
      end
      stitch_cmd += ["--images"] + images
      if copy_master_geometry_exactly
        stitch_cmd << "--copy-master-geometry-exactly"
      end
      align_to.each do |master|
        stitch_cmd += ["--master", master]
      end
      stitch_cmd << "--stitch-quit"
      stitch_cmd << "--batch-mode"
      stitch_cmd << "--xvfb"
      stitch_cmd << "--nolog"

      rules << Rule.new(["#{target_prefix}.gigapan"], 
                        align_to, 
                        [stitch_cmd.join(" "),
                         "mv #{target_prefix}-#{suffix}.gigapan #{target_prefix}.gigapan",
                         "mv #{target_prefix}-#{suffix}.data #{target_prefix}.data"
                        ])
    end
    rules
  end    
end

class Compiler
  attr_reader :source, :tiles_dir, :transpose_dir, :videosets_dir, :urls
  # attr_reader :id, :versioned_id
  
  def to_s
    "#<Compiler name=#{name} width=#{@width} height=#{@height}>"
  end
  
  def initialize(settings)
    @urls = settings["urls"] || {}
    # @id = settings["id"] || raise("Time Machine must have unique ID")
    # @version = settings["version"] || raise("Time Machine must have version")
    # @versioned_id = "#{@id}-v#{@version}"
    # @label = settings["label"] || raise("Time Machine must have label")
    # STDERR.puts "Timemachine #{@id} (#{@label})"

    @tiles_dir="0200-tiles"
    @transpose_dir="0300-transpose"

    source_info = settings["source"] || raise("Time Machine must have source")
    initialize_source(source_info)

    initialize_transpose

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
    initialize_tiles
  end

  def initialize_source(source_info)
    sourcetypes = {
      "gigapan.org"=>GigapanOrgSource,
      "images"=>ImagesSource,
      "prestitched"=>PrestitchedSource,
      "stitch"=>StitchSource
    };
    
    sourcetypes[source_info["type"]] or raise "Source type must be one of #{sourcetypes.keys.join(" ")}"
    @source = sourcetypes[source_info["type"]].new(self, source_info)
    open("autogenerated_framenames.txt", "w") { |fh| fh.write @source.framenames.join("\n") }
  end

  def initialize_transpose
    FileUtils.mkdir_p @transpose_dir
    r = {'width' => @source.width, 'height' => @source.height,
         'tile_width' => @source.tilesize, 'tile_height' => @source.tilesize}
    open("#{@transpose_dir}/r.json", 'w') {|fh| fh.puts(JSON.pretty_generate(r))}
  end

  def initialize_destination(destination_info)
    if destination_info.class == String
      @videosets_dir = destination_info
    else
      @videosets_parent_dir="0400-videosets"
      @videosets_dir="#{@videosets_parent_dir}/#{@versioned_id}"
      if ! File.exists? @videosets_parent_dir
        case destination_info["type"]
        when "local"
          Dir.mkdir @videosets_parent_dir
        when "symlink"
          target = destination_info["target"] or raise("Must specify 'target' for 'symlink' in 'destination'")
          File.exists? target or raise("'target' path #{target} does not exist")
          File.symlink(target, @videosets_parent_dir)
        else
          raise "Destination type not recognized"
        end
      end
    end
    FileUtils.mkdir_p @videosets_dir
    ['css', 'images', 'js', 'player_template.html', 'time_warp_composer.html'].each do |src|
      FileUtils.cp_r "#{$explorer_source_dir}/#{src}", @videosets_dir
    end
    FileUtils.cp "#{$explorer_source_dir}/integrated-viewer.html", "#{@videosets_dir}/view.html"
  end

  def initialize_tiles
    @all_tiles = Tile.enumerate(@source.width, @source.height, @source.tilesize)
    @max_level = @all_tiles.map{|tile| tile.level}.max
    @base_tiles = @all_tiles.find_all{|tile| tile.level == @max_level}
    @subsampled_tiles = @all_tiles.find_all{|tile| tile.level < @max_level}
  end

  def all_tiles_rule
    tileset_targets = @source.tiles_rules.map{|rule| rule.targets}.flatten(1)
    
    target = "#{@tiles_dir}/COMPLETE"
    Rule.new([target], tileset_targets, ["touch #{target}"])
  end

  def transpose_path(tile)
    "#{@transpose_dir}/#{tile.path}.ts2"
  end

  def transpose_rule(tile, dependencies)
    children = tile.children & @all_tiles
    if children.size > 0
      subsampled_transpose_rule(tile, children, dependencies)
    else
      base_transpose_rule(tile, dependencies)
    end
  end

  def subsampled_transpose_rule(target_idx, children, dependencies)
    target = transpose_path(target_idx)
    children = children.map{|child| transpose_rule(child, dependencies).targets}.flatten(1)

    cmd = [$tilestacktool]
    frames = {'frames' => {'start' => 0, 'end' => @source.framenames.size - 1}, 
              'bounds' => target_idx.bounds(@source.tilesize, @max_level)}
    cmd += ['--path2stack', @source.tilesize, @source.tilesize, "'#{JSON.generate(frames)}'", @transpose_dir]
    cmd += ['--save', target]
    Rule.new([target], children, [cmd.join(' ')])
  end

  def base_transpose_rule(target, dependencies)
    inputs = @source.framenames.map {|frame| "#{@tiles_dir}/#{frame}.data/tiles/#{target.path}.#{@source.tileformat}"}
    target = transpose_path(target)
    cmd = [$tilestacktool] # , "--delete-source-tiles"]
    cmd += ["--loadtiles"] + inputs
    cmd += ["--create-parent-directories", "--save", target]
    Rule.new([target], dependencies, [cmd.join(" ")])
  end

  def all_transposes_rule
    atr = all_tiles_rule
    STDERR.puts "   #{@base_tiles.size} base tiles per input frame (#{@source.tilesize}x#{@source.tilesize} px)"
    target = "#{@transpose_dir}/COMPLETE"
    Rule.new([target], transpose_rule(Tile.new(0,0,0), atr.targets).targets, ["touch #{target}"])
  end
    
  # def transpose_cleanup_rule
  #   #remove remaining .jpgs leftover and all sub directories
  #   #leave the top-most directory (*.data/tiles/r.info)
  #   #do not do anything if subsmaple is not 1 for the tiling
  #   cmd = @@global_parent.source.subsample == 1 ? ["find #{@tiles_dir} -name *.jpg | xargs rm -f", 
  #                                                  "find #{@tiles_dir} -type d | xargs rmdir --ignore-fail-on-non-empty"] : ["echo"]
  #   dependencies = all_transposes_rule.targets
  #   Rule.new(["transpose-cleanup"], dependencies, 
  #            cmd, 
  #            {:local=>true})
  # end
  
  def videoset_rules
    # dependencies=transpose_cleanup_rule.targets
    dependencies = all_transposes_rule.targets
    @videoset_compilers.map{|vc| vc.rules(dependencies)}.flatten(1)
  end

  def capture_times_rule()
    dependencies = videoset_rules.map{|rule| rule.targets}.flatten(1)
    cmd = ["ruby #{@@global_parent.source.capture_time_parser}",
           "#{@@global_parent.source.capture_time_parser_inputs}",
           "#{@videosets_dir}/tm.json"]
    Rule.new(["capture_times"], dependencies, [cmd.join(" ")])
  end

  def howto_rule
    # dependencies = capture_times_rule.targets
    dependencies = videoset_rules.map{|rule| rule.targets}.flatten(1)
    directions = []
    directions << "echo 'Add this to #{@urls['view'] || "your page"}: {{TimeWarpComposer}} {{TimelapseViewer|timelapse_id=#{@versioned_id}|timelapse_dataset=1}}'"
    if @urls['track']
      directions << "echo 'and update tracking page #{@urls['track']}'"
    end
    Rule.new(["howto"], dependencies, 
             directions,
             {:local=>true})
  end

  def compute_rules
    howto_rule
  end

  def info
    {
      "base-id"  => @versioned_id,
      "name"     => @label,
      "datasets" => @videoset_compilers.map do |vc| 
        {
          "id" => vc.id,
          "name" => vc.label
        }
      end,
      "sizes" => @videoset_compilers.map(&:label)

    }
  end

  def write_json
    dest = "#{@videosets_dir}/tm.json"
    system "mkdir -p #{@videosets_dir}"
    open(dest, "w") do |fh|
      fh.puts(JSON.pretty_generate(info))
    end
    @videoset_compilers.each { |vc| vc.write_json }
    STDERR.puts("Wrote #{dest}")
  end
end

def write_makefile
  # # Group rules
  # rules_by_dependencies={}
  # 
  # Rule.all.each do |rule| 
  #   rules_by_dependencies[rule.dependencies] ||= []
  #   rules_by_dependencies[rule.dependencies] << rule 
  # end
  # 
  #grouped_rules=[]
  #rules_by_dependencies.keys.each do |dependencies|
  #  rules = rules_by_dependencies[dependencies]
  #  groupsize = [rules.size / 100, 50].min
  #  groupsize = [groupsize, 1].max
  #  puts "#{rules.size} rules for dependencies #{dependencies.to_s[0...50]}...; grouping by #{groupsize}"
  #  rules.each_slice(groupsize) do |slice|
  #    targets = []
  #    commands = []
  #    slice.each do |rule|
  #      targets += rule.targets
  #      commands += rule.commands
  #    end
  #    grouped_rules << Rule.new(targets, dependencies, commands)
  #  end
  #end
  grouped_rules = Rule.all

  #STDERR.puts "Combined #{Rule.all.size-grouped_rules.size} rules to #{grouped_rules.size}"
  
  all_targets = grouped_rules.map{|rule| rule.targets}.flatten(1)
  
  open("rules.txt","w") do |makefile|
    makefile.puts "# generated by ct.rb"
    makefile.puts
    makefile.puts "all: #{all_targets.join(" ")}"
    makefile.puts
    
    grouped_rules.each do |rule|
      makefile.puts rule.to_make
      makefile.puts
    end
    
  end
  
  # open("monitorfiles.txt","w") do |fh|
  #   fh.puts all_targets.join(" ")
  # end
end

class Maker
	@@ndone = 0
	
  def initialize(rules)
    @rules = rules
    @ready = []
    @targets={}
    @rules_waiting = {}
    @status = {}
    STDERR.write "Checking targets from #{rules.size} rules...  0%"
    rules.each_with_index do |rule, i| 
      if i*100/rules.size > (i-1)*100/rules.size
        STDERR.write("\b\b\b\b%3d%%" % (i*100/rules.size))
      end
      check_rule(rule)
    end
    STDERR.write("\b\b\b\bdone.\n")
  end

	def self.ndone
		@@ndone
	end

	def self.reset_ndone
		@@ndone = 0
	end

  def check_rule(rule)
    if @status[rule] == :done
      return
    elsif rule_targets_exist(rule)
      rule_completed(rule)
    elsif @status[rule] == :ready
      return
    elsif @status[rule] == :executing
      return
    else
      rule_ready = true
      rule.dependencies.each do |dep|
        if not target_exists?(dep)
          (@rules_waiting[dep] ||= Set.new) << rule
          rule_ready = false
        end
      end
      if rule_ready
        @status[rule] = :ready
        @ready << rule
      else
        @status[rule] = :waiting
      end
    end
  end

  def target_exists?(target)
    if @targets.member?(target)
      @targets[target]
    else
      @targets[target] = File.exists?(target)
    end
  end

  def rule_targets_exist(rule)
    rule.targets.each do |target|
      target_exists?(target) or return false
    end
    true
  end

  def rule_completed(rule)
    @status[rule] == :done and raise("assert")
    @status[rule] == :ready and raise("assert")
    @@ndone += 1
    @status[rule] = :done
    rule.targets.each do |target|
      if not @targets[target]
        @targets[target] = true
        if @rules_waiting.member?(target)
          @rules_waiting[target].each {|rule| check_rule(rule)}
        end
      end
    end
  end  

  def execute_rules(job_no, rules, response)
    counter = 1;
    result = 1;
    commands = rules.map{|rule| rule.commands}.flatten(1)
    command = commands.join(" && ")
    if !@local && !rules.map{|rule| rule.local}.reduce{|a,b| a and b}
      command = "submit_synchronous '#{command}'"
    end
    
    STDERR.write "#{date} Job #{job_no} executing #{command}\n"
    
    # Retry up to 3 times if we fail
    while (!(result = system(command)) && counter < 4) do
      STDERR.write "#{date} Job #{job_no} failed, retrying, attempt #{counter}\n"
      counter += 1
    end
    
    if !result
      STDERR.write "#{date} Job #{job_no} failed too many times; aborting\n"
      @aborting = true
      response.push([job_no, [], Thread.current])
    else
      STDERR.write "#{date} Job #{job_no} completed successfully\n"
      response.push([job_no, rules, Thread.current])
    end
  end

  def make(max_jobs, max_rules_per_job, local=false)
    @local = local
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
    
    @aborting = false
    current_job_no = 0
    initial_ndone = @@ndone

    print_status(rules_executing, nil)
    
    while @@ndone < @rules.size
    	# Restart the process if we are stitching from source and we have not gotten dimensions from the first gigapan
      if @@global_parent.source.class.to_s == "StitchSource" && @@ndone > 0 && @@global_parent.source.width == 1 && @@global_parent.source.height == 1
        STDERR.write "Initial .gigapan file created. We now have enough info to get the dimensions.\n"
        STDERR.write "Removing old make and monitor file...\n"
        system("rm Makefile monitorfiles.txt")
        STDERR.write "Clearing all old rules...\n"
        Rule.clear_all
        Maker.reset_ndone
        STDERR.write "Restarting process with new dimensions...\n"
        return
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
        exit 1
      end
      if @ready.empty? && jobs_executing == 0
        STDERR.write "#{date}: Don't know how to build targets, aborting"
        exit 1
      end
      print_status(rules_executing, jobs_executing)
      (job_no, rules, thread) = response.pop
      thread.join
      jobs_executing -= 1
      rules_executing -= rules.size
      rules.each {|rule| rule_completed(rule)}
      print_status(rules_executing, jobs_executing)
    end
    duration = Time.now.to_i - begin_time.to_i
    STDERR.write "#{date}: Completed #{@rules.size-initial_ndone} rules (#{current_job_no} jobs) in #{duration/86400}d #{duration%86400/3600}h #{duration%3600/60}m #{duration%60}s\n"
    if @aborting
      STDERR.write "Aborted due to error"
    end
  end

  def print_status(rules, jobs)
    status = []
    status << "#{(@@ndone*100.0/@rules.size).round(1)}% rules finished (#{@@ndone}/#{@rules.size})"
    rules && jobs && status << "#{rules} rules executing (in #{jobs} jobs)"
    status << "#{@ready.size} rules ready to execute"
    status << "#{@rules.size-@@ndone-@ready.size-rules} rules awaiting dependencies"
    STDERR.write "#{date} Rules #{status.join(". ")}\n"
  end

end

njobs = 75
rules_per_job = 10
local = false
retry_attempts = 0
destination = nil

$tilestacktool = "tilestacktool"
$explorer_source_dir = File.expand_path "#{File.dirname(__FILE__)}/time-machine-explorer"
jsonfile = ""

while !ARGV.empty?
  arg = ARGV.shift
  if arg == '-j'
    njobs = ARGV.shift.to_i
  elsif arg == '-r'
    rules_per_job  = ARGV.shift.to_i
  elsif arg == '-v'
    $verbose = true
  elsif arg == '-l'
    local = true
    njobs = 1
    rules_per_job = 1
  elsif arg == '--tilestacktool'
    $tilestacktool = File.expand_path ARGV.shift
  elsif arg == '--create'
    destination = File.expand_path ARGV.shift
  elsif File.extname(arg) == '.timemachinedefinition'
    if File.directory?(arg)
      arg = "#{arg}/definition.timemachinedefinition"
    end
    jsonfile = arg
  else
    STDERR.puts "Unknown arg #{arg}"
    usage
  end
end

if !destination
  raise "Must specify --destination"
end

if `uname`.chomp == 'Darwin'
  local = true
end

jsonfile ||= File.exists?('definition.timemachinedefinition') && 'definition.timemachinedefinition'
jsonfile ||= 'default.json'

Dir.chdir(File.dirname(jsonfile))
jsonfile = File.basename(jsonfile)
definition = open(jsonfile) {|fh| JSON.load(fh)}
definition['destination'] = destination

while ((Maker.ndone == 0 || Maker.ndone < Rule.all.size) && retry_attempts < 3)
  compiler = Compiler.new(definition)
  compiler.write_json
  compiler.compute_rules # Creates rules, which will be accessible from Rule.all
  write_makefile
  Maker.new(Rule.all).make(njobs, rules_per_job, local)
  retry_attempts += 1
end

# result = RubyProf.stop
# printer = RubyProf::GraphPrinter.new(result)
# printer.print(open("5.txt","w"),{})
# exit 0
