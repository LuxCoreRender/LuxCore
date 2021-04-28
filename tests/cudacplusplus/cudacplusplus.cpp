#include <iostream>
#include <vector>

#include "luxrays/utils/cudacpp.h"

using namespace std;
using namespace luxrays;

//------------------------------------------------------------------------------
// DevicesInfo()
//------------------------------------------------------------------------------

void DevicesInfo() {
	int count;

	cudaGetDeviceCount(&count);
	CUDACPP_CHECKERROR();

	for (int i = 0; i < count; i++) {
		cudaDeviceProp prop;
		cudaGetDeviceProperties(&prop, i);
		CUDACPP_CHECKERROR();

		cout << "Device Number: " << i << endl;
		cout << "  Device name: " << prop.name << endl;
		cout << "  Memory Clock Rate (KHz): " << prop.memoryClockRate << endl;
		cout << "  Memory Bus Width (bits): " << prop.memoryBusWidth << endl;
		cout << "  Compute Capability: " << prop.major << "." << prop.minor <<endl;
	}
}
  
//------------------------------------------------------------------------------
// ClassTest()
//------------------------------------------------------------------------------

class VectorTest {
public:
	__host__ __device__ VectorTest(float _x = 0.f, float _y = 0.f, float _z = 0.f) :
		x(_x), y(_y), z(_z) {
	}
		
	__host__ __device__ VectorTest operator+(const VectorTest &v) const {
		return VectorTest(x + v.x, y + v.y, z + v.z);
	}
	
	__host__ __device__ bool operator!=(const VectorTest &v) const {
		return (x != v.x) || (y != v.y) || (z != v.z);
	}

	float x, y, z;
};

__global__ void SumVectorTest(VectorTest *va, VectorTest *vb, VectorTest *vc) {
	u_int index = blockIdx.x * blockDim.x + threadIdx.x;

	vc[index] = va[index] + vb[index];
}

inline ostream &operator<<(std::ostream &os, const VectorTest &v) {
	os << "VectorTest[" << v.x << ", " << v.y << ", " << v.z << "]";
	return os;
}

void ClassTest() {
	cout << "Class test..." << endl;

	const size_t size = 1024;

	VectorTest *va = CudaCPPNew<VectorTest>(size);
	VectorTest *vb = CudaCPPNew<VectorTest>(size);
	VectorTest *vc = CudaCPPNew<VectorTest>(size);

	for (u_int i = 0; i < size; ++i) {
		va[i] = VectorTest(i, 0.f, 0.f);
		vb[i] = VectorTest(0.f, i, 0.f);
		vc[i] = VectorTest(0.f, 0.f, 0.f);
	}

	SumVectorTest<<<size / 32, 32>>>(&va[0], &vb[0], &vc[0]);
	CUDACPP_CHECKERROR();

	cudaDeviceSynchronize();
	CUDACPP_CHECKERROR();

	for (u_int i = 0; i < size; ++i) {
		if (vc[i] != va[i] + vb[i])
			cout << "Failed index: " << i << endl;
		
		//cout << vc[i] << " = " << va[i] << " + " << vb[i] << endl;
	}

	CudaCPPDelete(va, size);
	CudaCPPDelete(vb, size);
	CudaCPPDelete(vc, size);

	cout << "Done" << endl;
}

//------------------------------------------------------------------------------

int main(int argc, char ** argv) {
	cout << "CUDA C++ Tests" << endl;

	DevicesInfo();
	//cudaSetDevice(0);
	//CUDACPP_CHECKERROR();
	
	ClassTest();

	return 0;
}
