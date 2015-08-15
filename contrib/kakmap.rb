#!/usr/bin/env ruby

# Generate a reference sheet for Kakoune's normal mode
# Use: ./kakmap.rb ../src/normal.cc

require 'markaby'

# Relies on the cmds array assignment ending with };
raw = ARGF.read.split( /cmds\[\] =\s+{\s*/m ).last.split( /^};$/ ).first

commands = {}

# break code into lines
raw.split( /\n+/ ).each{ |line|
  line.gsub!( /(^\s*{\s*|\s*},?\*$)/, '' ) # discard wrapping for array elements

  mod = (line.scan( /^alt|^ctrl/ ).first || 'none').to_sym
  key = line.scan(/(?:^Key::(\w+)|(?<!\\)'\\?(.*?)(?<!\\)')/).flatten.compact.first
  des = line.scan(/(?<!\\)"(?<desc>.*?)(?<!\\)"/).flatten.first

  key = 'Space' if key == ' '

  commands[key] ||= {}
  commands[key][mod] = des
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
  table do
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
        tr do
          th key
          td binding[:none]
          td binding[:alt]
          td binding[:ctrl]
        end
      end
    end
  end
}

