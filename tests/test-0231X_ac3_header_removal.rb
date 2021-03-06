#!/usr/bin/ruby -w

class T_0231X_ac3_header_removal < Test
  def description
    return "mkvextract / raw AC-3 with header removal / out(AC-3)"
  end

  def run
    xtr_tracks "data/ac3/ac3_header_removal.mka", "0:#{tmp}"
    return hash_tmp
  end
end
