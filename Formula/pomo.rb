class Pomo < Formula
  desc "Terminal pomodoro timer with color config persistence"
  homepage "https://github.com/BrigBryu/pomo"
  url      "https://github.com/BrigBryu/pomo/archive/refs/tags/v1.0.4.tar.gz"
  sha256   "7c55e9c6557ca8195ebc81ca4023b9d5ddef6fdb0f5095132ae1c58dd5cb7cfe"
  license  "MIT"

  def install
    system "gcc", "-std=c11", "-O2", "pomo.c", "-o", "pomo"
    bin.install "pomo"
  end

  test do
    assert_match "Usage:", shell_output("#{bin}/pomo --help")
  end
end
