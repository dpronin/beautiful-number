#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>

#include <algorithm>
#include <concepts>
#include <future>
#include <iostream>
#include <iterator>
#include <memory>
#include <new>
#include <numeric>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

namespace {

auto constexpr kAsyncCountFactor = std::size_t{600000};

template <std::unsigned_integral T> constexpr T mul(T l, T r) {
  T x;
  if (__builtin_umull_overflow(l, r, &x))
    throw std::overflow_error("multiplication result of '" + std::to_string(l) +
                              "' and '" + std::to_string(r) +
                              "' cannot be presented");
  return x;
}

template <std::unsigned_integral T> constexpr T plus(T l, T r) {
  T x;
  if (__builtin_add_overflow(l, r, &x))
    throw std::overflow_error("addition result of '" + std::to_string(l) +
                              "' and '" + std::to_string(r) +
                              "' cannot be presented");
  return x;
}

template <std::unsigned_integral T> T parse(std::string_view arg) {
  T r;

  char *p_end;

  errno = 0;
  r = std::strtoul(arg.data(), &p_end, 10);
  if (errno || p_end < (arg.data() + arg.size())) {
    auto msg =
        "could not convert '" + std::string{arg} + "' into decimal value";
    if (errno) {
      msg += ", reason: ";
      msg += std::strerror(errno);
    }
    throw std::invalid_argument(std::move(msg));
  }

  return r;
}

template <std::unsigned_integral T>
constexpr std::span<T> work(T v_min, T v_max, T base,
                            std::span<T> sums) noexcept {
  assert(0 != base);
  /*
   * start from maximum value up to 0 to count up sums of digits it consists
   * of
   */
  for (auto v = v_min; v <= v_max; ++v) {
    auto x = T{};
    for (auto t = v; t;) {
      x += t % base;
      t /= base;
    }
    assert(x < sums.size());
    ++sums[x];
  }

  return sums;
}

template <std::unsigned_integral T>
constexpr T v_max_compute(T base, T order) noexcept(
    noexcept(plus(T{}, T{})) && noexcept(mul(T{}, T{}))) {
  /*
   * count a maximum value that the number with @base and @order
   * could take
   */
  auto v = T{};
  for (auto m = T{1}; order;
       v = plus(v, mul(m, base - 1)), m = mul(m, base), --order)
    ;
  return v;
}

template <std::unsigned_integral T>
constexpr T div_round_up(T v, T d) noexcept {
  assert(0 != d);
  return (v + d - 1) / d;
}

} // namespace

int main(int argc, char const *argv[]) try {
  if (argc < 3) {
    std::cerr << "call ./task <base> <order>\n";
    return EXIT_FAILURE;
  }

  auto result = UINT64_C(0);

  /* parse 'base' defining a power (number of distinct values) of one digit */
  auto const base = parse<uint64_t>(argv[1]);

  /* parse order of magnitude defining number of digits */
  auto const order = parse<uint64_t>(argv[2]);

  if (base > 0 && order > 0) {
    /* we may consider only half of values to fasten task completion */
    auto const half_order = order / 2;

    /*
     * maximum sum that could give sum of digits is maximum value of one digit
     * multiplied by number of digits under considiration
     */
    auto const max_sum = mul(base - 1, half_order);

    /* minimal value to take into account is zero */
    auto v_min = UINT64_C(0);

    /* compute a maximum value based on @base and @half_order */
    auto const v_max = v_max_compute(base, half_order);
    /*
     * number of values to take into account is a distance between maximum and
     * minimal value plus one
     */
    auto const v_count = v_max - v_min + 1;

    /* use C++ to learn how many concurrent HW-threads we have at the host */
    auto const parallel_nr_max = static_cast<uint64_t>(
        std::max(std::thread::hardware_concurrency(), 1u));

    std::vector<std::future<std::span<uint64_t>>> asyncs(
        std::min(div_round_up(v_count, kAsyncCountFactor), parallel_nr_max) -
        1u);

    /* number of works is a sum of number of asynchronous works and the work in
     * main thread to be run */
    auto const works_nr = asyncs.size() + 1u;

#ifdef __cpp_lib_hardware_interference_size
    auto const u64_chunks_nr_per_cache_line = div_round_up(
        std::hardware_destructive_interference_size, sizeof(uint64_t));
#elif defined(__x86_64__) || defined(_M_X64)
    auto const u64_chunks_nr_per_cache_line = UINT64_C(8);
#else
    /* let it be 1 by default for other architectures to meet maximum
     * performance to memory consumption detriment */
    auto const u64_chunks_nr_per_cache_line =
        div_round_up(sizeof(std::max_align_t), sizeof(uint64_t));
#endif

    auto const u64_chunks_nr_per_work =
        div_round_up(max_sum + 1, u64_chunks_nr_per_cache_line) *
        u64_chunks_nr_per_cache_line;

    /* buffer for temporary sums that works will compute */
    auto buffer_sums =
        std::make_unique<uint64_t[]>(mul(u64_chunks_nr_per_work, works_nr));

    /*
     * assess if there is an asynchronous work required to speed up the overall
     * calculations
     */
    if (!(works_nr < 2)) {
      /* Step is how many values must be processed by one asynchronous work */
      auto const v_step = v_count / works_nr;
      for (uint64_t async_work_nr = 0; async_work_nr < asyncs.size();
           ++async_work_nr, v_min = v_min + v_step) {
        /* run asynchronous works, each doing computations with its own range
         * of values
         *
         * array of distinct sums that could give sum of digits, from 0 up to
         * @max_sum inclusively
         */
        asyncs[async_work_nr] = std::async(
            std::launch::async, work<uint64_t>, v_min, v_min + v_step - 1, base,
            std::span{
                &buffer_sums[u64_chunks_nr_per_work * async_work_nr],
                max_sum + 1,
            });
      }
    }

    /* main thread is also supposed to do some worthwhile work */
    auto result_sums =
        work(v_min, v_max, base,
             std::span{
                 &buffer_sums[u64_chunks_nr_per_work * (works_nr - 1)],
                 max_sum + 1,
             });

    /* wait until all asynchronous works have finished their computations */
    while (!asyncs.empty()) {
      if (auto ait = std::ranges::find_if(
              asyncs,
              [](auto const &async) {
                using namespace std::chrono_literals;
                /*
                 * don't wait for any time, only
                 * check if task is ready and return
                 */
                return std::future_status::ready == async.wait_for(0s);
              });
          ait != asyncs.end()) [[likely]] {

        /*
         * as an asynchronous work has finished, we accumulate its results
         * with results of main thread
         */
        std::ranges::transform(result_sums, ait->get(), result_sums.begin(),
                               plus<uint64_t>);
        /*
         * throw away the outworked asynchronous task, wait the next or finish
         * with the cycle
         */
        std::iter_swap(ait, std::end(asyncs) - 1);
        asyncs.pop_back();
      } else {
        std::this_thread::yield();
      }
    }

    /* result is a sum of all distinct sums, squared on-going */
    result = std::transform_reduce(result_sums.begin(), result_sums.end(),
                                   result_sums.begin(), UINT64_C(0),
                                   plus<uint64_t>, mul<uint64_t>);

    /* check if order is odd */
    if (order & 0x1)
      result = mul(result, base);
  }

  std::cout << result << std::endl;

  return 0;
} catch (std::exception const &ex) {
  std::cerr << "exception occurred: " << ex.what() << "\n";
  return EXIT_FAILURE;
} catch (...) {
  std::cerr << "unknown exception occurred\n";
  return EXIT_FAILURE;
}
