#define Type_Box                0
#define Type_Gaussian           1
#define Type_Binomial           2

#define Mode_2d                 0
#define Mode_2dBilinear         1
#define Mode_Separable          2
#define Mode_SeparableBilinear  3
#define Mode_Prefilter          4

#ifndef TYPE
	#error TYPE not defined
#endif
#ifndef MODE
	#error MODE not defined
#endif
#ifndef KERNEL_SIZE
	#error KERNEL_SIZE not defined
#endif