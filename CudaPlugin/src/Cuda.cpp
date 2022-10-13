#include "Cuda.h"

Cuda::Cuda()
{
}

Cuda::~Cuda()
{
}

extern bool cuda_init();
extern void cuda_convert_uyvy_to_rgb(int w, int h, const uint8_t *s, uint8_t *d);
extern void cuda_convert_yuyv_to_rgb(int w, int h, const uint8_t *s, uint8_t *d);
extern void cuda_convert_uyvy_to_gray(int w, int h, const uint8_t *s, uint8_t *d);
extern void cuda_convert_yuyv_to_gray(int w, int h, const uint8_t *s, uint8_t *d);

bool Cuda::init()
{
	return cuda_init();
}

void Cuda::convert_uyvy_to_rgb(int w, int h, const uint8_t *src, uint8_t *dst)
{
	cuda_convert_uyvy_to_rgb(w, h, src, dst);
}

void Cuda::convert_yuyv_to_rgb(int w, int h, const uint8_t *src, uint8_t *dst)
{
	cuda_convert_yuyv_to_rgb(w, h, src, dst);
}

void Cuda::convert_uyvy_to_gray(int w, int h, const uint8_t *src, uint8_t *dst)
{
	cuda_convert_uyvy_to_gray(w, h, src, dst);
}

void Cuda::convert_yuyv_to_gray(int w, int h, const uint8_t *src, uint8_t *dst)
{
	cuda_convert_yuyv_to_gray(w, h, src, dst);
}

