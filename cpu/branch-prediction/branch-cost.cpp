#include <chrono>
#include <cstring>
#include <iostream>
#include <random>
#include <vector>

constexpr long long kSize = 100'000'000;

/*
   Isolates the cost of misprediction from any data or cache effect.

   Both modes allocate the same size vector<char>, fill every byte
   sequentially, and read every byte sequentially in the exact same
   order during the timed loop. Memory footprint, access pattern, and
   amount of work per element are identical between the two modes. The
   only thing that differs is the VALUE stored at each position, which
   controls which way the branch goes. Since cache behavior depends on
   access pattern and footprint, not on the values being read, cache
   behavior is the same in both modes by construction. Any timing
   difference between them comes from branch prediction, not memory.

   predictable: every value is 1, so the branch goes the same direction
   every single time. The branch predictor locks onto this almost
   immediately and stays right for the entire run.

   random: each value is decided by an independent fair coin flip. No
   predictor can do better than roughly 50% accuracy against a
   genuinely unpredictable sequence, regardless of how good the
   prediction hardware is.

   vector<char> is used rather than vector<bool>, deliberately.
   vector<bool> is a bit-packed specialization in the standard library,
   reading from it involves shifting and masking bits out of a packed
   word, which adds overhead unrelated to branching itself. vector<char>
   gives a plain byte per element with no extra unpacking cost, keeping
   memory access uniform between the two modes as well.
*/

long long run(const std::vector<char>& decisions) {
    long long total = 0;

    for (char take_branch : decisions) {
        if (take_branch) {
            total += 1;
        } else {
            total += 2;
        }
    }

    return total;
}

int main(int argc, char** argv) {
    if (argc != 2 || (std::strcmp(argv[1], "predictable") != 0 && std::strcmp(argv[1], "random") != 0)) {
        std::cout << "Usage: " << argv[0] << " [predictable|random]\n";
        return 1;
    }

    std::vector<char> decisions(kSize);

    /*
       Each mode runs in its own process invocation, never combined in
       one run, the same separation used for every other good versus bad
       comparison in this repo.
    */
    if (std::strcmp(argv[1], "predictable") == 0) {
        for (long long i = 0; i < kSize; ++i) {
            decisions[i] = 1;
        }
    } else {
        std::mt19937 rng(12345);
        std::bernoulli_distribution coin(0.5);
        for (long long i = 0; i < kSize; ++i) {
            decisions[i] = coin(rng) ? 1 : 0;
        }
    }

    auto start = std::chrono::steady_clock::now();
    long long result = run(decisions);
    auto end = std::chrono::steady_clock::now();

    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    std::cout << argv[1] << ": " << ms << " ms, result = " << result << "\n";

    return 0;
}
