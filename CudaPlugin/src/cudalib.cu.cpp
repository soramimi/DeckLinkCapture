#include <stdio.h>
#include <stdint.h>

__global__ void kernel(unsigned char *p)
{
//	int i = blockIdx.x;
//	int j = blockIdx.y;
	int i = threadIdx.x;
	int j = threadIdx.y;
	p[i * 9 + j] = (i + 1) * (j + 1);
}

bool cuda_init()
{
	unsigned char table[81];
	unsigned char *mem;
	cudaMalloc((void **)&mem, 81);
//	dim3 b(9, 9);
	dim3 t(9, 9);
	kernel<<<1,t>>>(mem);
	cudaMemcpy(table, mem, 81, cudaMemcpyDeviceToHost);
	cudaFree(mem);

	for (int i = 0; i < 9; i++) {
		for (int j = 0; j < 9; j++) {
			if (table[i * 9 + j] != (i + 1) * (j + 1)) {
				return false;
			}
		}
	}

	return true;
}

__device__ uint8_t clamp_uint8(int v)
{
	return v < 0 ? 0 : (v > 255 ? 255 : v);
}

__device__ uint8_t gray(int r, int g, int b)
{
	return clamp_uint8((r * 306 + g * 601 + b * 117) / 1024);
}

__global__ void cu_convert_uyvy_to_rgb(int w, int h, const uint8_t *s, uint8_t *d)
{
	int x = blockIdx.x * blockDim.x + threadIdx.x;
	int y = blockIdx.y * blockDim.y + threadIdx.y;

	uint8_t U  = s[4 * w * y + 4 * x + 0];
	uint8_t Y0 = s[4 * w * y + 4 * x + 1];
	uint8_t V  = s[4 * w * y + 4 * x + 2];
	uint8_t Y1 = s[4 * w * y + 4 * x + 3];
	int R0 = ((Y0 - 16) * 1192 +                    (V - 128) * 1634) / 1024;
	int G0 = ((Y0 - 16) * 1192 - (U - 128) * 400  - (V - 128) * 832 ) / 1024;
	int B0 = ((Y0 - 16) * 1192 + (U - 128) * 2065                   ) / 1024;
	int R1 = ((Y0 - 16) * 1192 +                    (V - 128) * 1634) / 1024;
	int G1 = ((Y0 - 16) * 1192 - (U - 128) * 400  - (V - 128) * 832 ) / 1024;
	int B1 = ((Y0 - 16) * 1192 + (U - 128) * 2065                   ) / 1024;
	d[6 * w * y + 6 * x + 0] = clamp_uint8(R0);
	d[6 * w * y + 6 * x + 1] = clamp_uint8(G0);
	d[6 * w * y + 6 * x + 2] = clamp_uint8(B0);
	d[6 * w * y + 6 * x + 3] = clamp_uint8(R1);
	d[6 * w * y + 6 * x + 4] = clamp_uint8(G1);
	d[6 * w * y + 6 * x + 5] = clamp_uint8(B1);
}

void cuda_convert_uyvy_to_rgb(int w, int h, const uint8_t *s, uint8_t *d)
{
	uint8_t *src;
	uint8_t *dst;
	cudaMalloc((void **)&src, w * h * 2);
	cudaMalloc((void **)&dst, w * h * 3);
	dim3 b((w / 2 + 15) / 16, (h + 15) / 16);
	dim3 t(16, 16);
	cudaMemcpy(src, s, w * h * 2, cudaMemcpyHostToDevice);
	cu_convert_uyvy_to_rgb<<<b,t>>>(w / 2, h, src, dst);
	cudaMemcpy(d, dst, w * h * 3, cudaMemcpyDeviceToHost);
	cudaFree(src);
	cudaFree(dst);
}

__global__ void cu_convert_yuyv_to_rgb(int w, int h, const uint8_t *s, uint8_t *d)
{
	int x = blockIdx.x * blockDim.x + threadIdx.x;
	int y = blockIdx.y * blockDim.y + threadIdx.y;

	uint8_t Y0 = s[4 * w * y + 4 * x + 0];
	uint8_t U  = s[4 * w * y + 4 * x + 1];
	uint8_t Y1 = s[4 * w * y + 4 * x + 2];
	uint8_t V  = s[4 * w * y + 4 * x + 3];
	int R0 = ((Y0 - 16) * 1192 +                    (V - 128) * 1634) / 1024;
	int G0 = ((Y0 - 16) * 1192 - (U - 128) * 400  - (V - 128) * 832 ) / 1024;
	int B0 = ((Y0 - 16) * 1192 + (U - 128) * 2065                   ) / 1024;
	int R1 = ((Y0 - 16) * 1192 +                    (V - 128) * 1634) / 1024;
	int G1 = ((Y0 - 16) * 1192 - (U - 128) * 400  - (V - 128) * 832 ) / 1024;
	int B1 = ((Y0 - 16) * 1192 + (U - 128) * 2065                   ) / 1024;
	d[6 * w * y + 6 * x + 0] = clamp_uint8(R0);
	d[6 * w * y + 6 * x + 1] = clamp_uint8(G0);
	d[6 * w * y + 6 * x + 2] = clamp_uint8(B0);
	d[6 * w * y + 6 * x + 3] = clamp_uint8(R1);
	d[6 * w * y + 6 * x + 4] = clamp_uint8(G1);
	d[6 * w * y + 6 * x + 5] = clamp_uint8(B1);
}

void cuda_convert_yuyv_to_rgb(int w, int h, const uint8_t *s, uint8_t *d)
{
	uint8_t *src;
	uint8_t *dst;
	cudaMalloc((void **)&src, w * h * 2);
	cudaMalloc((void **)&dst, w * h * 3);
	dim3 b((w / 2 + 15) / 16, (h + 15) / 16);
	dim3 t(16, 16);
	cudaMemcpy(src, s, w * h * 2, cudaMemcpyHostToDevice);
	cu_convert_yuyv_to_rgb<<<b,t>>>(w / 2, h, src, dst);
	cudaMemcpy(d, dst, w * h * 3, cudaMemcpyDeviceToHost);
	cudaFree(src);
	cudaFree(dst);
}


__global__ void cu_convert_uyvy_to_gray(int w, int h, const uint8_t *s, uint8_t *d)
{
	int x = blockIdx.x * blockDim.x + threadIdx.x;
	int y = blockIdx.y * blockDim.y + threadIdx.y;

	uint8_t U  = s[4 * w * y + 4 * x + 0];
	uint8_t Y0 = s[4 * w * y + 4 * x + 1];
	uint8_t V  = s[4 * w * y + 4 * x + 2];
	uint8_t Y1 = s[4 * w * y + 4 * x + 3];
	int R0 = ((Y0 - 16) * 1192 +                    (V - 128) * 1634) / 1024;
	int G0 = ((Y0 - 16) * 1192 - (U - 128) * 400  - (V - 128) * 832 ) / 1024;
	int B0 = ((Y0 - 16) * 1192 + (U - 128) * 2065                   ) / 1024;
	int R1 = ((Y0 - 16) * 1192 +                    (V - 128) * 1634) / 1024;
	int G1 = ((Y0 - 16) * 1192 - (U - 128) * 400  - (V - 128) * 832 ) / 1024;
	int B1 = ((Y0 - 16) * 1192 + (U - 128) * 2065                   ) / 1024;
	d[2 * w * y + 2 * x + 0] = gray(R0, G0, B0);
	d[2 * w * y + 2 * x + 1] = gray(R1, G1, B1);
}

void cuda_convert_uyvy_to_gray(int w, int h, const uint8_t *s, uint8_t *d)
{
	uint8_t *src;
	uint8_t *dst;
	cudaMalloc((void **)&src, w * h * 2);
	cudaMalloc((void **)&dst, w * h);
	dim3 b((w / 2 + 15) / 16, (h + 15) / 16);
	dim3 t(16, 16);
	cudaMemcpy(src, s, w * h * 2, cudaMemcpyHostToDevice);
	cu_convert_uyvy_to_gray<<<b,t>>>(w / 2, h, src, dst);
	cudaMemcpy(d, dst, w * h, cudaMemcpyDeviceToHost);
	cudaFree(src);
	cudaFree(dst);
}

__global__ void cu_convert_yuyv_to_gray(int w, int h, const uint8_t *s, uint8_t *d)
{
	int x = blockIdx.x * blockDim.x + threadIdx.x;
	int y = blockIdx.y * blockDim.y + threadIdx.y;

	uint8_t Y0 = s[4 * w * y + 4 * x + 0];
	uint8_t U  = s[4 * w * y + 4 * x + 1];
	uint8_t Y1 = s[4 * w * y + 4 * x + 2];
	uint8_t V  = s[4 * w * y + 4 * x + 3];
	int R0 = ((Y0 - 16) * 1192 +                    (V - 128) * 1634) / 1024;
	int G0 = ((Y0 - 16) * 1192 - (U - 128) * 400  - (V - 128) * 832 ) / 1024;
	int B0 = ((Y0 - 16) * 1192 + (U - 128) * 2065                   ) / 1024;
	int R1 = ((Y0 - 16) * 1192 +                    (V - 128) * 1634) / 1024;
	int G1 = ((Y0 - 16) * 1192 - (U - 128) * 400  - (V - 128) * 832 ) / 1024;
	int B1 = ((Y0 - 16) * 1192 + (U - 128) * 2065                   ) / 1024;
	d[2 * w * y + 2 * x + 0] = gray(R0, G0, B0);
	d[2 * w * y + 2 * x + 1] = gray(R1, G1, B1);
}

void cuda_convert_yuyv_to_gray(int w, int h, const uint8_t *s, uint8_t *d)
{
	uint8_t *src;
	uint8_t *dst;
	cudaMalloc((void **)&src, w * h * 2);
	cudaMalloc((void **)&dst, w * h);
	dim3 b((w / 2 + 15) / 16, (h + 15) / 16);
	dim3 t(16, 16);
	cudaMemcpy(src, s, w * h * 2, cudaMemcpyHostToDevice);
	cu_convert_yuyv_to_gray<<<b,t>>>(w / 2, h, src, dst);
	cudaMemcpy(d, dst, w * h, cudaMemcpyDeviceToHost);
	cudaFree(src);
	cudaFree(dst);
}












