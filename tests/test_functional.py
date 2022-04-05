# Copyright (c) Facebook, Inc. and its affiliates. 
#   
# This source code is licensed under the MIT license found in the 
# LICENSE file in the root directory of this source tree.
import pytest
import math
import torch
import bitsandbytes as bnb
import numpy as np
import time

from itertools import product

from bitsandbytes import functional as F

def setup():
    pass

def teardown():
    pass

@pytest.mark.parametrize("dtype", [torch.float32, torch.float16], ids=['float', 'half'])
def test_estimate_quantiles(dtype):
    A = torch.rand(1024, 1024, device='cuda')
    A = A.to(dtype)
    code = F.estimate_quantiles(A, normalize=False)

    percs = torch.linspace(1/512, 511/512, 256, device=A.device)
    torch.testing.assert_allclose(percs, code, atol=1e-3, rtol=1e-2)

    A = torch.randn(1024, 1024, device='cuda')
    A = A.to(dtype)
    code = F.estimate_quantiles(A, normalize=False)

    quantiles = torch.quantile(A.float(), percs)
    diff = torch.abs(code-quantiles)
    assert (diff > 5e-02).sum().item() == 0


def test_quantile_quantization():
    for i in range(100):
        A1 = torch.randn(1024, 1024, device='cuda')
        code = F.estimate_quantiles(A1)
        C2, S1 = F.quantize(A1, code=code)
        A2 = F.dequantize(C2, S1)
        diff = torch.abs(A1-A2).mean().item()
        assert diff < 0.0075

        A1 = torch.rand(1024, 1024, device='cuda')
        code = F.estimate_quantiles(A1)
        C2, S1 = F.quantize(A1, code=code)
        A2 = F.dequantize(C2, S1)
        diff = torch.abs(A1-A2).mean().item()
        torch.testing.assert_allclose(A1, A2, atol=5.5e-3, rtol=0)
        assert diff < 0.0011


def test_dynamic_blockwise_quantization():
    diffs = []
    reldiffs = []
    for i in range(100):
        A1 = torch.randn(1024, 1024, device='cuda')
        C, S = F.quantize(A1)
        A2 = F.dequantize(C, S)
        diff = torch.abs(A1-A2)
        reldiff = diff/torch.abs(A1+1e-8)
        diffs.append(diff.mean().item())
        reldiffs.append(reldiff.mean().item())
        assert diffs[-1] < 0.011
    print(sum(diffs)/len(diffs))
    print(sum(reldiffs)/len(reldiffs))

    diffs = []
    for i in range(100):
        A1 = torch.rand(1024, 1024, device='cuda')
        C, S = F.quantize(A1)
        A2 = F.dequantize(C, S)
        diff = torch.abs(A1-A2).mean().item()
        assert diff < 0.0033
        diffs.append(diff)
        torch.testing.assert_allclose(A1, A2, atol=1e-2, rtol=0)
    #print(sum(diffs)/len(diffs))

def test_dynamic_blockwise_stochastic_quantization():
    diffs = []
    reldiffs = []
    rand = torch.rand(1024).cuda()
    for i in range(100):
        A1 = torch.randn(1024, 1024, device='cuda')
        C1, S1 = F.quantize(A1, rand=rand)
        C2, S2 = F.quantize(A1)
        # a maximunm distance of quantized values of 1
        torch.testing.assert_allclose(C1, C2, atol=1, rtol=0)
        fraction_smaller = (C1<C2).float().sum()/C1.numel()
        fraction_larger = (C1>C2).float().sum()/C1.numel()
        torch.testing.assert_allclose(fraction_larger, fraction_smaller, atol=0.01, rtol=0)



@pytest.mark.parametrize("gtype", [torch.float32, torch.float16], ids=['float', 'half'])
def test_percentile_clipping(gtype):
    gnorm_vec1 = torch.zeros(100, device='cuda')
    gnorm_vec2 = torch.zeros(100, device='cuda')
    n = 4
    step = 0
    percentile=5
    for i in range(1000):
        step += 1
        g = torch.randn(n, n, dtype=gtype, device='cuda')
        gnorm1, clip2, gnorm_scale = F.percentile_clipping(g, gnorm_vec2, step, percentile=percentile)
        assert gnorm_scale == 1.0 if gnorm1 < clip2 else clip2/gnorm1

        gnorm2 = torch.norm(g.float())
        if step == 1:
            gnorm_vec1[:] = gnorm2
        else:
            gnorm_vec1[step % 100] = gnorm2

        vals, idx = torch.sort(gnorm_vec1)
        clip1 = vals[percentile]

        torch.testing.assert_allclose(gnorm_vec1, torch.sqrt(gnorm_vec2))
        torch.testing.assert_allclose(clip1, clip2)
        torch.testing.assert_allclose(gnorm1, gnorm2)


def test_stable_embedding():
    layer = bnb.nn.StableEmbedding(1024, 1024)
    layer.reset_parameters()


def test_dynamic_blockwise_quantization_cpu():
    #A1 = torch.randn(1024, 1024, device='cpu')
    #code = F.create_dynamic_map()
    #for i in range(1000):
    #    C, S = F.quantize_blockwise(A1, code=code)
    #    A2 = F.dequantize_blockwise(C, S)

    for i in range(10):
        # equivalence with GPU blockwise quantization
        A1 = torch.randn(1024, 1024, device='cpu')
        C1, S1 = F.quantize(A1, block_size=4096)
        C2, S2 = F.quantize(A1.cuda(), block_size=4096)
        torch.testing.assert_allclose(S1[0], S2[0].cpu())
        # there seems to be some issues with precision in CUDA vs CPU
        # not all elements are usually close, with couple off elements in a million
        print(C1.numel())
        print(C1)
        print(C2)
        idx = torch.isclose(C1, C2.cpu())
        assert (idx==0).sum().item() < 15


    diffs = []
    reldiffs = []
    for i in range(10):
        A1 = torch.randn(1024, 1024, device='cpu')
        C, S = F.quantize(A1)
        A2 = F.dequantize(C, S)
        diff = torch.abs(A1-A2)
        reldiff = diff/torch.abs(A1+1e-8)
        diffs.append(diff.mean().item())
        reldiffs.append(reldiff.mean().item())
        assert diffs[-1] < 0.011
    #print(sum(diffs)/len(diffs))
    #print(sum(reldiffs)/len(reldiffs))

    diffs = []
    for i in range(10):
        A1 = torch.rand(1024, 1024, device='cpu')
        C, S = F.quantize(A1)
        A2 = F.dequantize(C, S)
        diff = torch.abs(A1-A2).mean().item()
        assert diff < 0.0033
        diffs.append(diff)
        torch.testing.assert_allclose(A1, A2, atol=1e-2, rtol=0)
    #print(sum(diffs)/len(diffs))


def test_histogram():
    dim1, dim2 = 32, 32
    source = torch.rand(dim1, dim2, device='cuda')
    idx1 = torch.randint(0, 255, size=(dim1, dim2), device='cuda').int()
    idx2 = torch.randint(0, 255, size=(dim1, dim2), device='cuda').int()
    histogram1 = torch.zeros((256, 256)).cuda()
    histogram2 = torch.zeros((256, 256)).cuda()

    F.histogram_scatter_add_2d(histogram2, idx1, idx2, source)

    for i in range(dim1):
        for j in range(dim2):
            histogram1[idx1[i, j].item(), idx2[i, j].item()] += source[i, j]

    torch.testing.assert_allclose(histogram1, histogram2)
    torch.testing.assert_allclose(histogram1.sum(), source.sum())


k = 1
def test_managed():
    n = 1024*10
    A = F.get_managed(n, n, dtype=torch.float32)
    B = F.get_managed(n, n, dtype=torch.uint8)
    assert A.is_managed
    assert B.is_managed
    F.fill(A, 17.0)
    F.fill(B, 17)
    F.prefetch_cpu(A)
    F.prefetch_cpu(B)
    torch.cuda.synchronize()
    C = A*B.float()

    assert (A==17).sum().item() == n*n
    assert (B==17).sum().item() == n*n
    assert (C==289).sum().item() == n*n


def get_binary_code():
    values = list(product([0, 1], repeat=8))
    values.sort()

    data = []
    for j, v in enumerate(values):
        exp = 1
        for bit in v[1:]:
            if bit == 1: break
            exp += 1


        base = 8-1-1-exp
        value = 0
        for i, bit in enumerate(v[1+exp:]):
            if bit: value += 2**(base-i)
        base = 2**(base+1)-1
        if exp == 8:
            data.append((j, 0))
        elif exp == 0 or base == 0:
            data.append((j, (-1 if v[0] == 1 else 1)*1.0))
        else:
            #print(value, base)
            inc = 0.9/(base+1)
            linear_quant = (0.1+(inc*value)) + (inc/2.0)
            #if v[0] == 1: continue
            val = (-1 if v[0] == 1 else 1)*linear_quant*(10**(-exp+1))
            #print(j, value, linear_quant, val)
            data.append((j, val))

    #data.sort()
    #x = torch.Tensor(data)
    #print(x[:-1] - x[1:])
    data = [v[1] for v in data]
    return torch.Tensor(data).float()



k = 1000
def test_bench_bits():
    diffs = []
    reldiffs = []
    n = 1024
    A1 = torch.randn(n, n).cuda()
    for i in range(k):
        C2, S2 = F.quantize_blockwise_dynamic(A1)
        A2 = F.dequantize_blockwise_dynamic(C2, S2)

    torch.cuda.synchronize()
    t0 = time.time()
    for i in range(k):
        C2, S2 = F.quantize_blockwise_dynamic(A1, S2)
        F.dequantize_blockwise_dynamic(C2, S2, A2)
    torch.cuda.synchronize()
    print(time.time() - t0)

    C2, S2 = F.quantize(A1)
    A2 = F.dequantize(A1, S2[1], S2[0])
    torch.cuda.synchronize()
    t0 = time.time()
    for i in range(k):
        C2, S2 = F.quantize(A1, S2[1], S2[0])
        F.dequantize(A1, S2[1], S2[0], out=A2)
    torch.cuda.synchronize()
    print(time.time() - t0)

def test_blockwise_dynamic_bits():
    n = 1024
    diffs = []
    reldiffs = []
    for i in range(k):
        A1 = torch.randn(n, n, device='cuda')
        C2, S2 = F.quantize_blockwise_dynamic(A1)
        A2 = F.dequantize_blockwise_dynamic(C2, S2)

        diff = torch.abs(A1-A2)
        reldiff = diff/torch.abs(A1+1e-8)
        diffs.append(diff.mean().item())
        reldiffs.append(reldiff.mean().item())
        assert diffs[-1] < 0.011
    print(sum(diffs)/len(diffs))
    print(sum(reldiffs)/len(reldiffs))


def test_bench_cpu_blockwise():
    n = 1024
    torch.cuda.synchronize()
    A1 = torch.randn(n, n)
    t0 = time.time()
    for i in range(100):
        C2, S2 = F.quantize(A1, block_size=4096)
        A2 = F.dequantize(C2, S2)


    err1 = torch.abs(A1-A2).mean().item()
    assert err1 < 0.015
    print(time.time() - t0)
