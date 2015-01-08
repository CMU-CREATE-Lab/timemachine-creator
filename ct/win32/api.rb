case 
  when RUBY_VERSION =~ /1\.8/
    require File.join(File.dirname(__FILE__), 'ruby18/win32/api')
  when RUBY_VERSION =~ /1\.9/
    require File.join(File.dirname(__FILE__), 'ruby19/win32/api')
  when RUBY_VERSION =~ /2\.0/
    if RbConfig::CONFIG['arch'] =~ /x64/i
      require File.join(File.dirname(__FILE__), 'ruby2_64/win32/api')
    else
      require File.join(File.dirname(__FILE__), 'ruby2_32/win32/api')
    end
  when RUBY_VERSION =~ /2\.1/
    if RbConfig::CONFIG['arch'] =~ /x64/i
      require File.join(File.dirname(__FILE__), 'ruby21_64/win32/api')
    else
      require File.join(File.dirname(__FILE__), 'ruby21_32/win32/api')
    end
end
