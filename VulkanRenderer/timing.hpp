#include <chrono>

using timing_clock = std::chrono::high_resolution_clock;


template <typename T>
class Timing {

public:
  void tick() { tic_time = timing_clock::now(); }

  T tock()
  {
    auto dur = std::chrono::duration_cast<T>(timing_clock::now() - tic_time);
    tick();
    return dur;
  }

private:
  timing_clock::time_point tic_time = timing_clock::now();
};
