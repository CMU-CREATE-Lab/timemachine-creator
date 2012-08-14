class CheckMem
  def self.machine_stats
    meminfo = open("/proc/meminfo", "r") {|mem| mem.read}
    stats = {}
    meminfo.scan(/^(.*?):\s*(\d+)/) {|key, val| stats[key] = val.to_f/1000}
    stats
  end

  def self.process_stats
    meminfo = open("/proc/self/status", "r") {|mem| mem.read}
    stats = {}
    meminfo.scan(/^(.*?):\s*(\d+)/) {|key, val| stats[key] = val.to_f/1000}
    stats
  end

  def self.logvm(msg)
    STDERR.puts "VM=#{self.process_stats['VmSize']}MB. #{msg}.  #{Time.new}"
  end
end


