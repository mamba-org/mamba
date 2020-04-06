#pragma once

#include <indicators/color.hpp>
#include <indicators/termcolor.hpp>

#include <algorithm>
#include <chrono>
#include <iomanip>
#include <ostream>
#include <string>
#include <vector>

#include <cassert>
#include <cmath>

namespace indicators {
namespace details {

inline void set_stream_color(std::ostream &os, Color color) {
  switch (color) {
  case Color::grey:
    os << termcolor::grey;
    break;
  case Color::red:
    os << termcolor::red;
    break;
  case Color::green:
    os << termcolor::green;
    break;
  case Color::yellow:
    os << termcolor::yellow;
    break;
  case Color::blue:
    os << termcolor::blue;
    break;
  case Color::magenta:
    os << termcolor::magenta;
    break;
  case Color::cyan:
    os << termcolor::cyan;
    break;
  case Color::white:
    os << termcolor::white;
    break;
  default:
    assert(false);
  }
}

inline std::ostream &write_duration(std::ostream &os, std::chrono::nanoseconds ns) {
  using namespace std;
  using namespace std::chrono;
  using days = duration<int, ratio<86400>>;
  char fill = os.fill();
  os.fill('0');
  auto d = duration_cast<days>(ns);
  ns -= d;
  auto h = duration_cast<hours>(ns);
  ns -= h;
  auto m = duration_cast<minutes>(ns);
  ns -= m;
  auto s = duration_cast<seconds>(ns);
  if (d.count() > 0)
    os << setw(2) << d.count() << "d:";
  if (h.count() > 0)
    os << setw(2) << h.count() << "h:";
  os << setw(2) << m.count() << "m:" << setw(2) << s.count() << 's';
  os.fill(fill);
  return os;
}

class BlockProgressScaleWriter {
public:
  BlockProgressScaleWriter(std::ostream &os, size_t bar_width) : os(os), bar_width(bar_width) {}

  std::ostream &write(float progress) {
    std::string fill_text{"█"};
    std::vector<std::string> lead_characters{" ", "▏", "▎", "▍", "▌", "▋", "▊", "▉"};
    auto value = std::min(1.0f, std::max(0.0f, progress / 100.0f));
    auto whole_width = std::floor(value * bar_width);
    auto remainder_width = fmod((value * bar_width), 1.0f);
    auto part_width = std::floor(remainder_width * lead_characters.size());
    std::string lead_text = lead_characters[size_t(part_width)];
    if ((bar_width - whole_width - 1) < 0)
      lead_text = "";
    for (size_t i = 0; i < whole_width; ++i)
      os << fill_text;
    os << lead_text;
    for (size_t i = 0; i < (bar_width - whole_width - 1); ++i)
      os << " ";
    return os;
  }

private:
  std::ostream &os;
  size_t bar_width = 0;
};

class ProgressScaleWriter {
public:
  ProgressScaleWriter(std::ostream &os, size_t bar_width, const std::string &fill,
                      const std::string &lead, const std::string &remainder)
      : os(os), bar_width(bar_width), fill(fill), lead(lead), remainder(remainder) {}

  std::ostream &write(size_t progress) {
    auto pos = static_cast<size_t>(progress * bar_width / 100.0);
    for (size_t i = 0; i < bar_width; ++i) {
      if (i < pos)
        os << fill;
      else if (i == pos)
        os << lead;
      else
        os << remainder;
    }
    return os;
  }

private:
  std::ostream &os;
  size_t bar_width = 0;
  std::string fill;
  std::string lead;
  std::string remainder;
};

} // namespace details
} // namespace indicators
