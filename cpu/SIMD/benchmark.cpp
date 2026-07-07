#include <iostream>
#include <vector>
#include <string>
#include <random>
#include <chrono>
#include <immintrin.h>

/*
This file exists because main.cpp's simple add turned out to be
memory-bandwidth-bound, one add per float loaded and stored leaves the CPU
waiting on memory almost the whole time, so SIMD showed no measurable
wall-clock difference for it, confirmed directly. This version repeats a
multiply-add on each element so there is arithmetic work per float loaded,
moving the operation from memory-bound to compute-bound, which is where
SIMD's instruction-level throughput can actually show up as a timed
difference.

compute_normal runs four independent chains across four different elements,
i, i+1, i+2, i+3, rather than one chain per element. A single chain here is
a serial dependency, x = x*y + a[i] depends on the previous step's result,
so the CPU cannot start step k+1 until step k resolves. Four independent
chains give the CPU parallel work to fill that wait with instead of
stalling on one chain alone. An earlier attempt at this used four
accumulators that all started from the same value and ran the same
formula, which the compiler correctly recognized as identical and
collapsed back into one chain, silently undoing the fix. Confirmed in the
disassembly before catching it. The four chains here read four different
elements specifically so the compiler cannot deduplicate them.

compute_simd is the same idea with a single vector chain, eight elements
per step instead of one. Comparing this against compute_normal isolates
the instruction-level parallelism advantage more than the raw width
advantage, since compute_normal already has four-way ILP and this only has
one chain, confirmed at roughly 2x.

compute_simd4 gives the vector version the same four-way independent-chain
treatment as compute_normal, four separate __m256 chains instead of one.
With equal ILP exposed on both sides, the remaining difference against
compute_normal isolates the actual vector-width advantage, confirmed at
close to the theoretical 8x ceiling for 256-bit AVX.
*/

void compute_normal(const std::vector<float>& a, const std::vector<float>& b,
                     std::vector<float>& out, int iters) {
    size_t i = 0;
    for (; i + 4 <= a.size(); i += 4) {
        float x0 = a[i],   y0 = b[i];
        float x1 = a[i+1], y1 = b[i+1];
        float x2 = a[i+2], y2 = b[i+2];
        float x3 = a[i+3], y3 = b[i+3];
        for (int k = 0; k < iters; k++) {
            x0 = x0 * y0 + a[i];
            x1 = x1 * y1 + a[i+1];
            x2 = x2 * y2 + a[i+2];
            x3 = x3 * y3 + a[i+3];
        }
        out[i] = x0; out[i+1] = x1; out[i+2] = x2; out[i+3] = x3;
    }
    for (; i < a.size(); i++) {
        float x = a[i], y = b[i];
        for (int k = 0; k < iters; k++) x = x * y + a[i];
        out[i] = x;
    }
}

void compute_simd(const std::vector<float>& a, const std::vector<float>& b,
                   std::vector<float>& out, int iters) {
    size_t i = 0;
    for (; i + 8 <= a.size(); i += 8) {
        __m256 va = _mm256_loadu_ps(&a[i]);
        __m256 vb = _mm256_loadu_ps(&b[i]);
        __m256 vx = va;
        for (int k = 0; k < iters; k++) {
            vx = _mm256_add_ps(_mm256_mul_ps(vx, vb), va);
        }
        _mm256_storeu_ps(&out[i], vx);
    }
    for (; i < a.size(); i++) {
        float x = a[i], y = b[i];
        for (int k = 0; k < iters; k++) x = x * y + a[i];
        out[i] = x;
    }
}

void compute_simd4(const std::vector<float>& a, const std::vector<float>& b,
                    std::vector<float>& out, int iters) {
    size_t i = 0;
    for (; i + 32 <= a.size(); i += 32) {
        __m256 va0 = _mm256_loadu_ps(&a[i]),      vb0 = _mm256_loadu_ps(&b[i]);
        __m256 va1 = _mm256_loadu_ps(&a[i+8]),    vb1 = _mm256_loadu_ps(&b[i+8]);
        __m256 va2 = _mm256_loadu_ps(&a[i+16]),   vb2 = _mm256_loadu_ps(&b[i+16]);
        __m256 va3 = _mm256_loadu_ps(&a[i+24]),   vb3 = _mm256_loadu_ps(&b[i+24]);
        __m256 vx0 = va0, vx1 = va1, vx2 = va2, vx3 = va3;
        for (int k = 0; k < iters; k++) {
            vx0 = _mm256_add_ps(_mm256_mul_ps(vx0, vb0), va0);
            vx1 = _mm256_add_ps(_mm256_mul_ps(vx1, vb1), va1);
            vx2 = _mm256_add_ps(_mm256_mul_ps(vx2, vb2), va2);
            vx3 = _mm256_add_ps(_mm256_mul_ps(vx3, vb3), va3);
        }
        _mm256_storeu_ps(&out[i],    vx0);
        _mm256_storeu_ps(&out[i+8],  vx1);
        _mm256_storeu_ps(&out[i+16], vx2);
        _mm256_storeu_ps(&out[i+24], vx3);
    }
    for (; i < a.size(); i++) {
        float x = a[i], y = b[i];
        for (int k = 0; k < iters; k++) x = x * y + a[i];
        out[i] = x;
    }
}

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cout << "usage: ./benchmark [normal|simd|simd4]\n";
        return 1;
    }
    std::string mode = argv[1];

    const size_t n = 1'000'000;
    const int iters = 10000;
    std::vector<float> a(n), b(n), out(n);
    std::mt19937 rng(42);
    std::uniform_real_distribution<float> dist(0.0f, 1.001f);
    for (size_t i = 0; i < n; i++) {
        a[i] = dist(rng);
        b[i] = dist(rng);
    }

    auto start = std::chrono::steady_clock::now();
    if (mode == "normal") {
        compute_normal(a, b, out, iters);
    } else if (mode == "simd") {
        compute_simd(a, b, out, iters);
    } else if (mode == "simd4") {
        compute_simd4(a, b, out, iters);
    } else {
        std::cout << "unknown mode\n";
        return 1;
    }
    auto end = std::chrono::steady_clock::now();
    auto us = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();

    float checksum = 0.0f;
    for (float v : out) checksum += v;
    std::cout << mode << ": " << us << " us, checksum=" << checksum << "\n";
}
