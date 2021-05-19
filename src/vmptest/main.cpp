#include <intrin.h>
#include <iostream>
#include <Windows.h>
#include "ia32.h"

int main()
{
	cpuid_eax_01 cpuid_info;
	__cpuid((int*)&cpuid_info, 1);

	if (cpuid_info.cpuid_feature_information_ecx.avx_support)
		return 1;

	return 0;
}