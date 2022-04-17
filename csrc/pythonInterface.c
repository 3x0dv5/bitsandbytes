// Copyright (c) Facebook, Inc. and its affiliates. 
//   
// This source code is licensed under the MIT license found in the 
// LICENSE file in the root directory of this source tree.

#include <ops.cuh>

// We cannot call templated code from C, so we wrap the template in a C compatible call here if necessary.
// We use macro functions to expand all the different optimizers. Looks ugly, and is ugly, but its better than to 
// maintain all that boilerplate
//===================================================================================
//                               UNMANGLED CALLS
//===================================================================================

void estimateQuantiles_fp32(float *A, float *code, float offset, int n){ estimateQuantiles<float>(A, code, offset, n); }
void estimateQuantiles_fp16(half *A, float *code, float offset, int n){ estimateQuantiles<half>(A, code, offset, n); }


#define MAKE_FUNC32(fname, oname, gtype, gbits) \
void fname##32bit_g##gbits(gtype *g, gtype *p, \
               float* state1, float* state2, float *unorm, float max_unorm, float param_norm, \
               const float beta1, const float beta2, const float eps, const float weight_decay, \
               const int step, const float lr, float gnorm_scale, bool skip_zeros, const int n) \
{ optimizer32bit<gtype, oname>(g, p, state1, state2, unorm, max_unorm, param_norm, beta1, beta2, eps, weight_decay, step, lr, gnorm_scale, skip_zeros, n); } \

MAKE_FUNC32(momentum, MOMENTUM, float, 32)
MAKE_FUNC32(momentum, MOMENTUM, half, 16)
MAKE_FUNC32(adam, ADAM, float, 32)
MAKE_FUNC32(adam, ADAM, half, 16)
MAKE_FUNC32(rmsprop, RMSPROP, float, 32)
MAKE_FUNC32(rmsprop, RMSPROP, half, 16)
MAKE_FUNC32(adagrad, ADAGRAD, float, 32)
MAKE_FUNC32(adagrad, ADAGRAD, half, 16)

#define MAKE_BLOCKWISE8(fname, optim_name, gtype, gbits) \
void fname##_8bit_blockwise_fp##gbits(gtype* p, gtype* g, \
                unsigned char* state1, unsigned char* state2, float beta1, float beta2, float eps, int step, float lr, \
                float* quantiles1, float* quantiles2, float* absmax1, float* absmax2, float weight_decay, const float gnorm_scale, bool skip_zeros, int n)\
{	optimizerStatic8bitBlockwise<gtype, optim_name>(p, g, state1, state2, beta1, beta2, eps, step, lr, quantiles1, quantiles2, absmax1, absmax2, weight_decay, gnorm_scale, skip_zeros, n); }\

MAKE_BLOCKWISE8(adam, ADAM, half, 16)
MAKE_BLOCKWISE8(adam, ADAM, float, 32)
MAKE_BLOCKWISE8(momentum, MOMENTUM, half, 16)
MAKE_BLOCKWISE8(momentum, MOMENTUM, float, 32)
MAKE_BLOCKWISE8(rmsprop, RMSPROP, half, 16)
MAKE_BLOCKWISE8(rmsprop, RMSPROP, float, 32)
MAKE_BLOCKWISE8(adagrad, ADAGRAD, half, 16)
MAKE_BLOCKWISE8(adagrad, ADAGRAD, float, 32)

#define MAKE_BLOCKWISE_DYNAMIC8(fname, optim_name, gtype, gbits) \
void fname##_8bit_blockwise_dynamic_fp##gbits(gtype* p, gtype* g, \
                unsigned char* state1, unsigned char* state2, float beta1, float beta2, float eps, int step, float lr, \
                float* absmax1, float* absmax2, float weight_decay, const float gnorm_scale, bool skip_zeros, int n)\
{	optimizer8bitBlockwiseDynamic<gtype, optim_name>(p, g, state1, state2, beta1, beta2, eps, step, lr, absmax1, absmax2, weight_decay, gnorm_scale, skip_zeros, n); }\

MAKE_BLOCKWISE_DYNAMIC8(adam, ADAM, half, 16)
MAKE_BLOCKWISE_DYNAMIC8(adam, ADAM, float, 32)
MAKE_BLOCKWISE_DYNAMIC8(momentum, MOMENTUM, half, 16)
MAKE_BLOCKWISE_DYNAMIC8(momentum, MOMENTUM, float, 32)
MAKE_BLOCKWISE_DYNAMIC8(rmsprop, RMSPROP, half, 16)
MAKE_BLOCKWISE_DYNAMIC8(rmsprop, RMSPROP, float, 32)
MAKE_BLOCKWISE_DYNAMIC8(adagrad, ADAGRAD, half, 16)
MAKE_BLOCKWISE_DYNAMIC8(adagrad, ADAGRAD, float, 32)


void percentileClipping_g32(float * g, float *gnorm_vec, int step, const int n){ percentileClipping<float>(g, gnorm_vec, step, n); }
void percentileClipping_g16(half * g, float *gnorm_vec, int step, const int n){ percentileClipping<half>(g, gnorm_vec, step, n); }

void quantizeBlockwise_fp16(float * code, half *A, float *absmax, unsigned char *out, const int n){ quantizeBlockwise<half, 0>(code, A, absmax, out, NULL, 0, n); }
void quantizeBlockwise_fp32(float * code, float *A, float *absmax, unsigned char *out, const int n){ quantizeBlockwise<float, 0>(code, A, absmax, out, NULL, 0, n); }
void quantizeBlockwise_stochastic_fp16(float * code, half *A, float *absmax, unsigned char *out, float* rand, int rand_offset, const int n){ quantizeBlockwise<half, 1>(code, A, absmax, out, rand, rand_offset, n); }
void quantizeBlockwise_stochastic_fp32(float * code, float *A, float *absmax, unsigned char *out, float* rand, int rand_offset, const int n){ quantizeBlockwise<float, 1>(code, A, absmax, out, rand, rand_offset, n); }

void dequantizeBlockwise_fp16(float *code, unsigned char *A, float *absmax, half *out, int blocksize, const int n){ dequantizeBlockwise<half>(code, A, absmax, out, blocksize, n); } \
void dequantizeBlockwise_fp32(float *code, unsigned char *A, float *absmax, float *out, int blocksize, const int n){ dequantizeBlockwise<float>(code, A, absmax, out, blocksize, n); }

#define MAKE_ELEMENTWISE_FUNC(fname, type_name, ctype, FUNC) \
void fname##_##type_name(ctype *ptr, ctype value, long n){ func<ctype, FUNC>(ptr, value, n); } \

MAKE_ELEMENTWISE_FUNC(fill, fp32, float, FILL)
MAKE_ELEMENTWISE_FUNC(fill, uint8, unsigned char, FILL)
MAKE_ELEMENTWISE_FUNC(arange, fp32, float, ARANGE)


#define MAKE_QUANT_BLOCKWISE_DYNAMIC(type_name, BLOCK_SIZE, dtype) \
void quantizeBlockwiseDynamic_##type_name##_##BLOCK_SIZE##b(dtype *A, float *absmax, unsigned char *out, bool is_signed, int n) \
{ quantizeBlockwiseDynamic<dtype, BLOCK_SIZE>(A, absmax, out, is_signed, n); } \

MAKE_QUANT_BLOCKWISE_DYNAMIC(fp32, 2048, float)
MAKE_QUANT_BLOCKWISE_DYNAMIC(fp32, 4096, float)
MAKE_QUANT_BLOCKWISE_DYNAMIC(fp16, 2048, half)
MAKE_QUANT_BLOCKWISE_DYNAMIC(fp16, 4096, half)

#define MAKE_DEQUANT_BLOCKWISE_DYNAMIC(type_name, BLOCK_SIZE, dtype) \
void dequantizeBlockwiseDynamic_##type_name##_##BLOCK_SIZE##b(unsigned char *A, float *absmax, dtype *out, bool is_signed, int n) \
{ dequantizeBlockwiseDynamic<dtype, BLOCK_SIZE>(A, absmax, out, is_signed, n); } \

MAKE_DEQUANT_BLOCKWISE_DYNAMIC(fp32, 2048, float)
MAKE_DEQUANT_BLOCKWISE_DYNAMIC(fp32, 4096, float)
MAKE_DEQUANT_BLOCKWISE_DYNAMIC(fp16, 2048, half)
MAKE_DEQUANT_BLOCKWISE_DYNAMIC(fp16, 4096, half)

extern "C"
{
	void cestimate_quantiles_fp32(float *A, float *code, float offset, int n){ estimateQuantiles_fp32(A, code, offset, n); }
	void cestimate_quantiles_fp16(half *A, float *code, float offset, int n){ estimateQuantiles_fp16(A, code, offset, n); }
  void cquantize_blockwise_fp16(float * code, half *A, float *absmax, unsigned char *out, const int n){ quantizeBlockwise_fp16(code, A, absmax, out, n); }
  void cquantize_blockwise_fp32(float * code, float *A, float *absmax, unsigned char *out, const int n){ quantizeBlockwise_fp32(code, A, absmax, out, n); }
  void cquantize_blockwise_stochastic_fp16(float * code, half *A, float *absmax, unsigned char *out, float *rand, int rand_offset, const int n){ quantizeBlockwise_stochastic_fp16(code, A, absmax, out, rand, rand_offset, n); }
  void cquantize_blockwise_stochastic_fp32(float * code, float *A, float *absmax, unsigned char *out, float *rand, int rand_offset, const int n){ quantizeBlockwise_stochastic_fp32(code, A, absmax, out, rand, rand_offset, n); }

  void cdequantize_blockwise_fp16(float *code, unsigned char *A, float *absmax, half *out, int blocksize, const int n){ dequantizeBlockwise_fp16(code, A, absmax, out, blocksize, n); }
  void cdequantize_blockwise_fp32(float *code, unsigned char *A, float *absmax, float *out, int blocksize, const int n){ dequantizeBlockwise_fp32(code, A, absmax, out, blocksize, n); }

	#define MAKE_CFUNC32(name, gtype, gbits) \
	void c##name##32bit_g##gbits(gtype *g, gtype *p, \
								 float* state1, float* state2, float *unorm, float max_unorm, float param_norm, \
								 const float beta1, const float beta2, const float eps, const float weight_decay, \
								 const int step, const float lr, const float gnorm_scale, bool skip_zeros, const int n) \
	{ name##32bit_g##gbits(g, p, state1, state2, unorm, max_unorm, param_norm, beta1, beta2, eps, weight_decay, step, lr, gnorm_scale, skip_zeros, n); } \

	MAKE_CFUNC32(adam, float, 32)
	MAKE_CFUNC32(adam, half, 16)
	MAKE_CFUNC32(momentum, float, 32)
	MAKE_CFUNC32(momentum, half, 16)
	MAKE_CFUNC32(rmsprop, float, 32)
	MAKE_CFUNC32(rmsprop, half, 16)
	MAKE_CFUNC32(adagrad, float, 32)
	MAKE_CFUNC32(adagrad, half, 16)

  #define MAKE_CBLOCKWISE8(fname, optim_name, gtype, gbits) \
  void c##fname##_8bit_blockwise_fp##gbits(gtype* p, gtype* g, \
                unsigned char* state1, unsigned char* state2, float beta1, float beta2, float eps, int step, float lr,  \
                float* quantiles1, float* quantiles2, float* absmax1, float* absmax2, float weight_decay, const float gnorm_scale, bool skip_zeros, int n) \
  {	fname##_8bit_blockwise_fp##gbits(p, g, state1, state2, beta1, beta2, eps, step, lr, quantiles1, quantiles2, absmax1, absmax2, weight_decay, gnorm_scale, skip_zeros, n); } \

	MAKE_CBLOCKWISE8(adam, ADAM, half, 16)
	MAKE_CBLOCKWISE8(adam, ADAM, float, 32)
	MAKE_CBLOCKWISE8(momentum, MOMENTUM, half, 16)
	MAKE_CBLOCKWISE8(momentum, MOMENTUM, float, 32)
	MAKE_CBLOCKWISE8(rmsprop, RMSPROP, half, 16)
	MAKE_CBLOCKWISE8(rmsprop, RMSPROP, float, 32)
	MAKE_CBLOCKWISE8(adagrad, ADAGRAD, half, 16)
	MAKE_CBLOCKWISE8(adagrad, ADAGRAD, float, 32)

  #define MAKE_CBLOCKWISE_DYNAMIC8(fname, optim_name, gtype, gbits) \
  void c##fname##_8bit_blockwise_dynamic_fp##gbits(gtype* p, gtype* g, \
                unsigned char* state1, unsigned char* state2, float beta1, float beta2, float eps, int step, float lr,  \
                float* absmax1, float* absmax2, float weight_decay, const float gnorm_scale, bool skip_zeros, int n) \
  {	fname##_8bit_blockwise_dynamic_fp##gbits(p, g, state1, state2, beta1, beta2, eps, step, lr, absmax1, absmax2, weight_decay, gnorm_scale, skip_zeros, n); } \

	MAKE_CBLOCKWISE_DYNAMIC8(adam, ADAM, half, 16)
	MAKE_CBLOCKWISE_DYNAMIC8(adam, ADAM, float, 32)
	MAKE_CBLOCKWISE_DYNAMIC8(momentum, MOMENTUM, half, 16)
	MAKE_CBLOCKWISE_DYNAMIC8(momentum, MOMENTUM, float, 32)
	MAKE_CBLOCKWISE_DYNAMIC8(rmsprop, RMSPROP, half, 16)
	MAKE_CBLOCKWISE_DYNAMIC8(rmsprop, RMSPROP, float, 32)
	MAKE_CBLOCKWISE_DYNAMIC8(adagrad, ADAGRAD, half, 16)
	MAKE_CBLOCKWISE_DYNAMIC8(adagrad, ADAGRAD, float, 32)


	void cpercentile_clipping_g32(float * g, float *gnorm_vec, int step, const int n){ percentileClipping_g32(g, gnorm_vec, step, n); }
	void cpercentile_clipping_g16(half * g, float *gnorm_vec, int step, const int n){ percentileClipping_g16(g, gnorm_vec, step, n); }

	void cquantize_blockwise_cpu_fp32(float *A, float *absmax, unsigned char *out, const int n){ quantize_cpu(A, absmax, out, n); }
	void cdequantize_blockwise_cpu_fp32(float *code, unsigned char *A, float *absmax, float *out, const int n){ dequantize_cpu(code, A, absmax, out, n); }

	void chistogram_scatter_add_2d(float* histogram, int *index1, int *index2, float *src, int maxidx1, int n){ histogramScatterAdd2D(histogram, index1, index2, src, maxidx1, n); }

	void *cget_managed_ptr(long rows, long cols, long dtype_size)
	{
		size_t bytes = rows*cols*dtype_size;
		void *ptr;
		CUDA_CHECK_RETURN(cudaMallocManaged(&ptr, bytes, cudaMemAttachHost));
		CUDA_CHECK_RETURN(cudaPeekAtLastError());

		return ptr;
	}

	void cprefetch(void *ptr, long n, long size_dtype, int device)
	{
		CUDA_CHECK_RETURN(cudaMemPrefetchAsync(ptr, n*size_dtype, device, NULL));
		CUDA_CHECK_RETURN(cudaPeekAtLastError());
	}

	void cfill_fp32(float *ptr, float fill_value, long n){ fill_fp32(ptr, fill_value, n); }
	void cfill_uint8(unsigned char *ptr, unsigned char fill_value, long n){ fill_uint8(ptr, fill_value, n); }
	void carange_fp32(float *ptr, float value, long n){ arange_fp32(ptr, value, n); }

#define MAKE_CQUANT_BLOCKWISE_DYNAMIC(type_name, BLOCK_SIZE, dtype) \
	void cquantize_blockwise_dynamic_##type_name##_##BLOCK_SIZE##b(dtype *A, float *absmax, unsigned char *out, bool is_signed, int n) \
	{ quantizeBlockwiseDynamic_##type_name##_##BLOCK_SIZE##b(A, absmax, out, is_signed, n); } 

#define MAKE_CDEQUANT_BLOCKWISE_DYNAMIC(type_name, BLOCK_SIZE, dtype) \
	void cdequantize_blockwise_dynamic_##type_name##_##BLOCK_SIZE##b(unsigned char *A, float *absmax, dtype *out, bool is_signed, int n) \
	{ dequantizeBlockwiseDynamic_##type_name##_##BLOCK_SIZE##b(A, absmax, out, is_signed, n); } 

	MAKE_CQUANT_BLOCKWISE_DYNAMIC(fp32, 2048, float)
	MAKE_CQUANT_BLOCKWISE_DYNAMIC(fp32, 4096, float)
	MAKE_CQUANT_BLOCKWISE_DYNAMIC(fp16, 2048, half)
	MAKE_CQUANT_BLOCKWISE_DYNAMIC(fp16, 4096, half)
	MAKE_CDEQUANT_BLOCKWISE_DYNAMIC(fp32, 2048, float)
	MAKE_CDEQUANT_BLOCKWISE_DYNAMIC(fp32, 4096, float)
	MAKE_CDEQUANT_BLOCKWISE_DYNAMIC(fp16, 2048, half)
	MAKE_CDEQUANT_BLOCKWISE_DYNAMIC(fp16, 4096, half)
}


