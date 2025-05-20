class Pomo < Formula
  desc "Terminal pomodoro timer with color config persistence"
  homepage "https://github.com/BrigBryu/pomo"
  url      "https://github.com/BrigBryu/pomo/archive/refs/tags/v1.0.4.tar.gz"
  sha256   "e6b284b1e4ef659d3908adc3386b7c10ee6e5678b0b639478219c90462ec534b"
  license  "MIT"

  def install
    system "gcc", "-std=c11", "-O2", "pomo.c", "-o", "pomo"
    bin.install "pomo"
  end

  test do
    assert_match "Usage:", shell_output("#{bin}/pomo --help")
  end
end
