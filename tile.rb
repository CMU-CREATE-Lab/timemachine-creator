class Tile
  attr_accessor :x, :y, :level
  def initialize(x, y, level)
    @x = x
    @y = y
    @level = level
  end

  def name
    ret = "r"
    (level-1).downto(0) do |i| 
      ret += (['0', '1', '2', '3'])[((@x >> i) & 1) + ((@y >> i) & 1)*2]
    end
    ret
  end

  def path
    ret = ""
    n = name
    0.step(n.size-4,3) {|i| ret += n[i ... i+3] + "/"}
    ret + n
  end
  
  def to_s
    "#<Tile x=#{@x} y=#{@y} level=#{@level} name='#{name}' path='#{path}'>"
  end

  def parent
    @level && Tile.new(@level - 1, @x / 2, @y / 2)
  end

  def children
    [Tile.new(@x * 2 + 0, @y * 2 + 0, @level + 1),
     Tile.new(@x * 2 + 1, @y * 2 + 0, @level + 1),
     Tile.new(@x * 2 + 0, @y * 2 + 1, @level + 1),
     Tile.new(@x * 2 + 1, @y * 2 + 1, @level + 1)]
  end

  def eql?(rhs)
    @x == rhs.x && @y == rhs.y && @level == rhs.level
  end

  def hash
    [@x, @y, @level].hash
  end

  def bounds(tilesize, maxlevel)
    scale = tilesize << (maxlevel - @level)
    return {'xmin' => @x * scale, 'ymin' => @y * scale, 'width' => scale, 'height' => scale}
  end

  def self.enumerate(width, height, tile_size)
    tiles = []
    max_level = 0
    while (tile_size << max_level) < [width, height].max
      max_level += 1
    end
    (max_level+1).times do |level|
      tile_size_at_level = tile_size << (max_level - level)
      (height / tile_size_at_level.to_f).ceil.times do |y|
        (width / tile_size_at_level.to_f).ceil.times do |x|
          tiles << Tile.new(x, y, level)
        end
      end
    end
    tiles
  end
end

