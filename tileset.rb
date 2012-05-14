class Tileset
  attr_reader :width, :height, :tile_size, :max_level

  def initialize(width, height, tile_size)
    @width = width
    @height = height
    @tile_size = tile_size
    @max_level = 0
    while (tile_size << @max_level) < [@width, @height].max
      @max_level += 1
    end
  end    
    
  def enumerate_level(level)
    (@height / tile_size_at_level(level).to_f).ceil.times.flat_map do |y|
      (@width / tile_size_at_level(level).to_f).ceil.times.map do |x|
        Tile.new(x, y, level)
      end
    end
  end
    
  def enumerate_max_level
    enumerate_level @max_level
  end

  def enumerate_subsampled_levels
    @max_level.times.flat_map {|level| enumerate_level(level)}
  end

  def enumerate
    (@max_level + 1).times.flat_map {|level| enumerate_level(level)}
  end

  def tile_size_at_level(level)
    tile_size_at_level = @tile_size << (@max_level - level)
  end

  def tile_bounds(tile)
    return [tile.x, tile.y, 1, 1].map {|x| x * tile_size_at_level(tile.level)}
  end
end

