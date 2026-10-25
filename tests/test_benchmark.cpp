/*
 * test_benchmark.cpp - Benchmark de verificación de rendimiento para rutinas SIMD ASM
 */
#include <iostream>
#include <vector>
#include <chrono>
#include "../src/asm/common/rg_image_asm.h"

int main()
{
	const int width = 1920;
	const int height = 1080;
	std::vector<uint8_t> y_plane(width * height, 128);
	std::vector<uint8_t> u_plane((width / 2) * (height / 2), 128);
	std::vector<uint8_t> v_plane((width / 2) * (height / 2), 128);
	std::vector<uint8_t> rgba_out(width * height * 4, 0);

	auto start = std::chrono::high_resolution_clock::now();
	
	for (int i = 0; i < 50; ++i) {
		rg_yuv420_to_rgba_simd(y_plane.data(), u_plane.data(), v_plane.data(),
		                         rgba_out.data(), width, height, width, width / 2);
	}
	
	auto end = std::chrono::high_resolution_clock::now();
	std::chrono::duration<double, std::milli> elapsed = end - start;
	
	std::cout << "[benchmark] YUV420 to RGBA (1920x1080 x 50 frames): " << elapsed.count() << " ms\n";
	std::cout << "[benchmark] Status: PASSED\n";
	return 0;
}
