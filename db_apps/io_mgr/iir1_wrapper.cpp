/*!
 * Copyright Notice:
 * Copyright (C) 2012 NetComm Wireless limited.
 *
 * This file or portions thereof may not be copied or distributed in any form
 * (including but not limited to printed or electronic forms and binary or object forms)
 * without the expressed written consent of NetComm Wireless Ltd.
 * Copyright laws and International Treaties protect the contents of this file.
 * Unauthorized use is prohibited.
 *
 *
 * THIS SOFTWARE IS PROVIDED BY NETCOMM WIRELESS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL NETCOMM
 * WIRELESS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OFF
 * SUCH DAMAGE.
 *
 */


/*
	wrapper functions to access template based C++ library of iir1
*/

#include <Iir.h>

const int order = 3;

extern "C" void lowpassfilter_destroy(void* clpf)
{
	Iir::Butterworth::LowPass<order>* lpf=(Iir::Butterworth::LowPass<order>*)clpf;
	
	delete lpf;
}

extern "C" void* lowpassfilter_create()
{
	Iir::Butterworth::LowPass<order>* lpf;
	
	lpf=new Iir::Butterworth::LowPass<order>();
	
	return (void*)lpf;
}

extern "C" void lowpassfilter_setup(void* clpf,double sampleRate,double cutoffFrequency)
{
	Iir::Butterworth::LowPass<order>* lpf=(Iir::Butterworth::LowPass<order>*)clpf;
	
	lpf->setup(order, sampleRate, cutoffFrequency);
}

extern "C" void lowpassfilter_reset(void* clpf)
{
	Iir::Butterworth::LowPass<order>* lpf=(Iir::Butterworth::LowPass<order>*)clpf;
	
	lpf->reset();
}

extern "C" double lowpassfilter_filter(void* clpf,double c)
{
	Iir::Butterworth::LowPass<order>* lpf=(Iir::Butterworth::LowPass<order>*)clpf;
	
	return lpf->filter(c);
}

#if 0
int test_main(int,char**)
{
	const int order = 3;
	Iir::Butterworth::LowPass<order> f;
	const float samplingrate = 1000; // Hz
	const float cutoff_frequency = 5; // Hz
	f.setup (order, samplingrate, cutoff_frequency);
	f.reset ();
	FILE *fimpulse = fopen("lpf.dat","wt");
	for(int i=0;i<1000;i++) 
	{
		float a=0;
		if (i==10) a = 1;
		float b = f.filter(a);
		fprintf(fimpulse,"%f\n",b);
	}
	fclose(fimpulse);
	
	return 0;
}
#endif
