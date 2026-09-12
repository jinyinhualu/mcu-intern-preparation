#include <cmath>
#include <iostream>

void addcmulCpu(const float* inputData,
                const float* x1,
                const float* x2,
                float value,
                float* y,
                int totalNum)
{
    int i = 0;
    while (i < totalNum) {
        y[i] = inputData[i] + x1[i] * x2[i] * value;
        ++i;
    }
}

int main()
{
    constexpr int totalNum = 3;
    const float inputData[totalNum] = {1.0f, 2.0f, 3.0f};
    const float x1[totalNum] = {2.0f, 3.0f, 4.0f};
    const float x2[totalNum] = {3.0f, 4.0f, 5.0f};
    constexpr float value = 0.5f;
    float y[totalNum] = {};

    addcmulCpu(inputData, x1, x2, value, y, totalNum);

    std::cout << "y = [";
    for (int i = 0; i < totalNum; ++i) {
        if (i > 0) {
            std::cout << ", ";
        }
        std::cout << y[i];
    }
    std::cout << "]\n";

    const float expected[totalNum] = {4.0f, 8.0f, 13.0f};
    for (int i = 0; i < totalNum; ++i) {
        if (std::fabs(y[i] - expected[i]) > 1e-6f) {
            std::cerr << "Test failed at index " << i << '\n';
            return 1;
        }
    }

    std::cout << "Test passed.\n";
    return 0;
}
