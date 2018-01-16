# Snd documentation index (generated by index.cl)

# html :play or html "play"  ==> the whole string must match
# html(/^play$/)             ==> regexp search

def html(str)
  url = if str.kind_of?(Regexp)
          # x --> ["play", "extsnd.html#sndplay"]
          pair = snd_urls().detect do |x| str.match(x[0]) end
          if pair.kind_of?(Array)
            pair[1]
          else
            false
          end
        else
          # String or Symbol
          snd_url(str.to_s)
        end
  if url and (not url.empty?)
    goto_html(url)
  else
    snd_print "no url for #{str}?"
    false
  end
end

def help(str)
  snd_help(str) or snd_help(html(str))
end

def goto_html(url)
  dir = [Dir.pwd,
         html_dir(),
         "/usr/doc/snd-11",
         "/usr/share/doc/snd-11",
         "/usr/local/share/doc/snd-11",
         "/usr/doc/snd-10",
         "/usr/share/doc/snd-10",
         "/usr/local/share/doc/snd-10",
         "/usr/doc/snd-8"].detect do |d|
    if d.kind_of?(String)
      if File.exist?(d + "/" + "snd.html")
        d
      end
    end
  end
  if dir
    case html_program()
    when "netscape", "mozilla"
      send_mozilla(dir + "/" + url)
    else
      system(format("%s file:%s/%s &", html_program(), dir, url))
    end
  else
    snd_print "snd.html not found; set_html_dir(path) to appropriate path!"
  end
  url
end