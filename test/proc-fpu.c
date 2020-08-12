#include <hal/if.h>
#include <main/if.h>
#include <proc/if.h>


#define LEN 128


int test_fpuThread1(void *arg)
{
	int i;
	double f[LEN];
	main_printf(ATTR_INFO,"test: [fpuThread1] start\n");

	f[0] = 1 - 0.0123456789;
	while (1) {
		for(i = 1;i < LEN;i++) {
			f[i] = f[i-1] * (1 - 0.00123456789);
		}
		for(i = 0;i < LEN;i++)
			if(f[i] >= 1 || f[i] <= 0)
				main_printf(ATTR_ERROR,"test: [fpuThread1] invalid result f[%d] = %f\n",i,f[i]);
		f[0] = f[LEN - 1];
		if(f[0] < 0.005)
			f[0] = 1 - 0.0123456789;
	}
	return 0;
}

int test_fpuThread2(void* arg)
{
	int i;
	double f[LEN];
	main_printf(ATTR_INFO,"test: [fpuThread2] start\n");
	f[0] = -1 + 0.0123456789;
	while (1) {
		for(i = 1;i < LEN;i++) {
			f[i] = -1* f[i-1] * (-1 + 0.00123456789);
		}
		for(i = 0;i < LEN;i++)
			if(f[i] <= -1 || f[i] >= 0)
				main_printf(ATTR_ERROR,"test: [fpuThread2] invalid result f[%d] = %f\n",i,f[i]);
		f[0] = f[LEN - 1];
		if(f[0] > -0.005)
			f[0] = -1 + 0.0123456789;
	}
	return 0;
}

void test_fpuContextSwitching(void)
{
	proc_thread(NULL, test_fpuThread1,NULL,0,NULL,ttRegular);
	proc_thread(NULL, test_fpuThread2,NULL,0,NULL,ttRegular);
}
