#!/usr/bin/env ruby

# Generate a reference sheet for Kakoune's normal mode
# Use: ./kakmap.rb ../src/normal.cc

require 'markaby'

# Relies on the keymap HashMap assignment ending with };
raw = ARGF.read.split( /const\s+HashMap<Key,\s*NormalCmd>\s+keymap\s*{/ ).last.split( /^};$/ ).first

commands = {}

raw.split( /\n+/ ).each{ |line|
  # skip empty or comment line
  line = line.strip
  if line.empty? or /^\/\// =~ line
    next
  end

  # match key mapping line
  /^\{\s*\{(?<mdky>[^}]+)\}\s*,\s*\{\s*"(?<dsc>[^"]+)"/.match(line) do |m|
    modAndKey = m['mdky']
    des = m['dsc']

    modAndKey.gsub!(/\s*\/\*[^*]+\*\/\s*/, '') # remove comment in key definition

    # match key and modifier
    /Key::(?<key>\w+)|(?<mod>alt|ctrl)\('\\?(?<key>.+?)'\)|'\\?(?<key>.+?)'$/.match(modAndKey) do |sm|
      key = sm['key']
      mod = (sm['mod'] || 'none').to_sym

      key = 'Space' if key == ' '
      commands[key] ||= {}
      commands[key][mod] = des
    end
  end
}

# sort, showing single characters first, symbols next and spelled out keys last
commands = commands.sort_by{ |key, _|
  case key
  when /^\w$/
    key.upcase + key.swapcase
  when /^\W$/
    '_' + key
  else
    '~~' + key
  end
}

puts Markaby::Builder.new {
  html do
    head do
      title "Kakoune default keymap"
    end
    body do
      table :style => "border-collapse: collapse" do
        thead do
          tr do
            th "Key"
            th "Description"
            th "ALT + key"
            th "CTRL + key"
          end
        end
        tbody do
          for key, binding in commands
            tr :style => "border-bottom: 1px solid #fbfbfb" do
              th key
              td binding[:none]
              td binding[:alt]
              td binding[:ctrl]
            end
          end
        end
      end
    end
  end
}

