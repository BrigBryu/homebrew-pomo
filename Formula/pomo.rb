class Pomo < Formula
  desc "Terminal pomodoro timer with color config persistence"
  homepage "https://github.com/BrigBryu/pomo"
  url      "https://github.com/BrigBryu/pomo/archive/refs/tags/v1.0.0.tar.gz"
  sha256   "4e49cbaaa40948c6b1dea7c77357e5314cda7450e77878238f21078e78dbb6bb"
  license  "MIT"

  def install
    system "gcc", "-std=c11", "-O2", "pomo.c", "-o", "pomo"
    bin.install "pomo"
  end

  test do
    assert_match "Usage:", shell_output("#{bin}/pomo --help")
  end
end
