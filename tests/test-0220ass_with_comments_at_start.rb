#!/usr/bin/ruby -w

class T_0220ass_with_comments_at_start < Test
  def description
    return "mkvmerge / ASS with comments at the start / in(ASS)"
  end

  def run
    merge("\"data/subtitles/ssa-ass/You're Under Arrest Movie 1.ass\"")
    return hash_tmp
  end
end

