#include <iostream>
#include <vector>
#include <immintrin.h>

std::vector<float> add_normal(const std::vector<float>& a, const std::vector<float>& b) {
    std::vector<float> out(a.size());
    for (size_t i = 0; i < a.size(); i++)
        out[i] = a[i] + b[i];
    return out;
}

std::vector<float> add_simd(const std::vector<float>& a, const std::vector<float>& b) {
    std::vector<float> out(a.size());
    size_t i = 0;
    for (; i + 8 <= a.size(); i += 8) {
        __m256 va = _mm256_loadu_ps(&a[i]);
        __m256 vb = _mm256_loadu_ps(&b[i]);
        __m256 vsum = _mm256_add_ps(va, vb);
        _mm256_storeu_ps(&out[i], vsum);
    }
    for (; i < a.size(); i++)
        out[i] = a[i] + b[i];
    return out;
}

int main() {
    std::vector<float> a = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f, 9.0f};
    std::vector<float> b = {10.0f, 10.0f, 10.0f, 10.0f, 10.0f, 10.0f, 10.0f, 10.0f, 10.0f};
    auto out_normal = add_normal(a, b);
    auto out_simd = add_simd(a, b);
    std::cout << "outputs match: " << (out_normal == out_simd ? "yes" : "no") << "\n";
}
