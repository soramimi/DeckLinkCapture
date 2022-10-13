#ifndef CUDA_H
#define CUDA_H

#include <QObject>
#include <stdint.h>

class Cuda : public QObject {
	Q_OBJECT
public:
	Cuda();
	~Cuda();

	virtual bool init();

	virtual void convert_uyvy_to_rgb(int w, int h, uint8_t const *src, uint8_t *dst);
	virtual void convert_yuyv_to_rgb(int w, int h, uint8_t const *src, uint8_t *dst);
	virtual void convert_uyvy_to_gray(int w, int h, uint8_t const *src, uint8_t *dst);
	virtual void convert_yuyv_to_gray(int w, int h, uint8_t const *src, uint8_t *dst);


};

#endif // CUDA_H
