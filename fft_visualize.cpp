#include<windows.h>
#include<chrono>
#include<pthread.h>
#include<stdio.h>
#include<fstream>
#include"fft.h"
#include"draw.h"
#include"wave.h"
using namespace std;
unsigned seek_amount=240000;
unsigned window_border_shift=38>>1;
unsigned long long window_size=2048;
long double peak_amp_decrease_speed=0.01;
inline long double mod(complex<long double> c)
{
	return sqrtl(c.real()*c.real()+c.imag()*c.imag());
} 
inline long double phase(complex<long double> c)
{
	return atan2(c.imag(),c.real());
}
vector<complex<long double>> l_buf,r_buf;
vector<complex<long double>> l_window_buf,r_window_buf;
vector<vector<complex<long double>>> l_result_buffer,r_result_buffer;
unsigned long long num_slides;
CRITICAL_SECTION l_result_buffer_lock,r_result_buffer_lock;
bool exit_flag=false;
//2 FFT threads, on a normal computer they should run ahead of the playing thread
//so you needn't worry that the FFT will be unable to catch up and crash the program
//this thread does FFT on the left channel
void* l_fft_thread(void*)
{
	for(unsigned long long i=0;i<num_slides;i++)
	{
		if(exit_flag)
			pthread_exit(NULL);
		l_window_buf.insert(l_window_buf.begin(),l_buf.begin()+i*window_size,l_buf.begin()+(i+1)*window_size);
		for(unsigned long long j=0;j<window_size;j++)
			l_window_buf[j]*=0.5*(sin(2*pi*j/(window_size-1))+1);
		EnterCriticalSection(&l_result_buffer_lock);
		l_result_buffer.push_back(fft(l_window_buf));
		LeaveCriticalSection(&l_result_buffer_lock);
		l_window_buf.clear();
	}
	return nullptr;
}
//this thread does FFT on the right channel
void* r_fft_thread(void*)
{
	for(unsigned long long i=0;i<num_slides;i++)
	{
		if(exit_flag)
			pthread_exit(NULL);
		r_window_buf.insert(r_window_buf.begin(),r_buf.begin()+i*window_size,r_buf.begin()+(i+1)*window_size);
		for(unsigned long long j=0;j<window_size;j++)
			r_window_buf[j]*=0.5*(sin(2*pi*j/(window_size-1))+1);
		EnterCriticalSection(&r_result_buffer_lock);
		r_result_buffer.push_back(fft(r_window_buf));
		LeaveCriticalSection(&r_result_buffer_lock);
		r_window_buf.clear();
	}
	return nullptr;
}
//convert HSV to RGB,H:0~360,s:0~100,v:0~100
//used for the rainbow color
COLORREF HSVtoRGB(int h,int s,int v)
{
	float RGB_min,RGB_max;
	RGB_max=v*2.55f;
	RGB_min=RGB_max*(100-s)/100.0f;
	int i=h/60;
	int difs=h%60;
	float RGB_Adj=(RGB_max-RGB_min)*difs/60.0f;
	int r,g,b;
	switch(i)
	{
		case 0:
			r=RGB_max;
			g=RGB_min+RGB_Adj;
			b=RGB_min;
			break;
		case 1:
			r=RGB_max-RGB_Adj;
			g=RGB_max;
			b=RGB_min;
			break;
		case 2:
			r=RGB_min;
			g=RGB_max;
			b=RGB_min+RGB_Adj;
			break;
		case 3:
			r=RGB_min;
			g=RGB_max-RGB_Adj;
			b=RGB_max;
			break;
		case 4:
			r=RGB_min+RGB_Adj;
			g=RGB_min;
			b=RGB_max;
			break;
		case 5:
			r=RGB_max;
			g=RGB_min;
			b=RGB_max-RGB_Adj;
			break;
	}
	return RGB(r,g,b);
}
//just a wrapper function,maps n:0~1 to a color, making a spectrum when n changes from 0~1(blue part is not included because it is too dark)
inline COLORREF get_color(long double n)
{
	return HSVtoRGB(n*200,100,100);
}
//draw a line on the memory canvas
void draw_line(int x,int y1,int y2,COLORREF c)
{
	HPEN hpen=CreatePen(PS_SOLID,1,c);
	SelectObject(canvas,hpen);
	MoveToEx(canvas,x,y1,NULL);
	LineTo(canvas,x,y2);
	DeleteObject(hpen);
}
string bin_file_name; 
bool audio_device_is_open=false;
//on exit, clean everything up
void exit_func()
{
	if(bin_file_name!="")
		DeleteFile(bin_file_name.c_str());
	if(audio_device_is_open)
		close_audio_device();
	exit_flag=true;
}
//on sigint, exit(will then call cleanup)
void on_sigint(int)
{
	exit(0);
}
int console_ctrl_handler(DWORD event)
{
	if(event==CTRL_CLOSE_EVENT)
	{
		exit_func();
		return true;
	}
	return false;
}
volatile int main_seek_frame;
volatile bool main_restart_flag=false,main_seek_flag=false,paused=false;
string filename;
unsigned long long bufsize;
short* short_buf;
//returns the current playing position in number of samples
unsigned long long get_pos()
{
	MMTIME mmtime;
	mmtime.wType=TIME_SAMPLES;
	waveOutGetPosition(out,&mmtime,sizeof(MMTIME));
	return mmtime.u.sample;
}
//currently unused, but preserved for seek function
//if needed, seek function may be added
//void seek(unsigned long long pos)
//{
//	waveOutReset(out);
//	short* temp_buf=new short[bufsize-(pos<<1)];
//	memcpy(temp_buf,short_buf,(bufsize<<1)-pos);
//	play_buffer(temp_buf,(bufsize>>1)-pos);
//	main_seek_frame=pos/window_size;
//	unsigned long long samples=get_pos();
//	while(get_pos()==samples);
//	main_seek_flag=true;
//	delete[] temp_buf;
//}
//currently listens for space(pause)
void* keyboard_listener_thread(void*)
{
	HWND hwnd=GetConsoleWindow();
	while(1)
	{
		if(GetForegroundWindow()==hwnd)
		{
			Sleep(5);
			{
				//deal with space
				static bool now=false,prev=false;
				prev=now;
				now=GetAsyncKeyState(VK_SPACE)&0x8000;
				if(now&&(!prev))
					paused=(!paused);
			}
		}
	}
}
//draw the spectrum, i is frame number i.e. index in fft window buffer 
void draw_bargraph(int i)
{
	static long double peak_amp=100;
	COLORREF c;
	int draw_end;
	EnterCriticalSection(&l_result_buffer_lock);
	for(unsigned long long j=0;j<(window_size>>1);j++)
	{
		c=get_color(j/(long double)(window_size>>1));
		if(mod(l_result_buffer[i][j])>=peak_amp)
			peak_amp=mod(l_result_buffer[i][j]);
		else
			if(peak_amp>=5)
				peak_amp-=peak_amp_decrease_speed;
		draw_end=min(max(256-mod(l_result_buffer[i][j])*256/peak_amp,0.0l),255.0l)+window_border_shift;
		draw_line(j,256+window_border_shift,draw_end,c);
	}
	LeaveCriticalSection(&l_result_buffer_lock);
	EnterCriticalSection(&r_result_buffer_lock);
	for(unsigned long long j=0;j<(window_size>>1);j++)
	{
		c=get_color(j/(long double)(window_size>>1));
		if(mod(r_result_buffer[i][j])>=peak_amp)
			peak_amp=mod(r_result_buffer[i][j]);
		else
			if(peak_amp>=5)
				peak_amp-=peak_amp_decrease_speed;
		draw_end=max(256+mod(r_result_buffer[i][j])*256/peak_amp,0.0l)+window_border_shift;
		draw_line(j,256+window_border_shift,draw_end,c);
	}
	LeaveCriticalSection(&r_result_buffer_lock);
}
//read file, convert it to PCM(if not already), start FFT threads, start playing audio and draw spectrum
int main(int argc,char** argv)
{
	if(argc==1)
	{
		cerr<<"visualize PCM audio\nusage:"<<argv[0]<<" <*.bin(raw PCM data)|*.*(any multimedia file with an audio stream)>";
		exit(0); 
	} 
	if(argc>=2)
	{
		atexit(exit_func);
		SetConsoleCtrlHandler(console_ctrl_handler,true);
		unsigned long long style; 
		HWND hwnd=GetConsoleWindow();
		style=GetWindowLongPtr(hwnd,GWL_STYLE);
		style&=(~WS_MAXIMIZEBOX);
		style|=WS_POPUP;
		SetWindowLongPtr(hwnd,GWL_STYLE,style);
		SetWindowText(hwnd,"FFT spectrum visualization");
		InitializeCriticalSection(&l_result_buffer_lock);
		InitializeCriticalSection(&r_result_buffer_lock);
		resize_window(1024/GetScalingFactor(),512);
		init_graphics();
		SetBkColor(canvas,RGB(0,0,0));
		SetTextColor(canvas,RGB(255,255,255));
		bool is_converted=false; 
		filename=argv[1];
		pthread_create(0,0,keyboard_listener_thread,0);
main_restart:
		if(filename.substr(filename.length()-4,4)!=".bin")
		{
			if(filename[0]=='\"'&&filename[filename.length()-1]=='\"')
			{
				system(("ffmpeg -i "+filename+" -f s16le -ar 48000 -ac 2 -y \""+filename.substr(1,filename.length()-2)+".bin\"").c_str());
				filename=filename.substr(1,filename.length()-2)+".bin";
			}
			else
			{
				system(("ffmpeg -i \""+filename+"\" -f s16le -ar 48000 -ac 2 -y \""+filename+".bin\"").c_str());
				filename=filename+".bin"; 
			}
			bin_file_name=filename;
			is_converted=true;
		}
		unsigned long long file_size=get_file_size(filename);
		if(file_size&3)
		{
			cout<<"file size is odd, should be even\n";
			exit(0);
		}
		vector<unsigned long long> fps_buffer; 
		bufsize=file_size>>1;
		cerr<<"reading PCM data...";
		short_buf=new short[bufsize];
		FILE* fp=fopen(filename.c_str(),"r");
		fread(short_buf,sizeof(short),bufsize,fp);
		fclose(fp);
		for(unsigned long long i=0;i<(bufsize>>1);i++)
		{
			l_buf.push_back(complex<long double>(short_buf[i<<1],0));
			r_buf.push_back(complex<long double>(short_buf[i<<1|1],0));	
		}
		cerr<<"done\n";
		if(is_converted)
			DeleteFile(filename.c_str());
		bin_file_name="";
		num_slides=ceil((bufsize>>1)/(long double)window_size);
		unsigned long long n=num_slides*window_size;
		unsigned long long d=n-(bufsize>>1);
		for(unsigned long long i=0;i<d;i++)
		{
			l_buf.push_back(complex<long double>(0,0));
			r_buf.push_back(complex<long double>(0,0));
		}
		cerr<<"starting FFT...\n";
		pthread_create(0,0,l_fft_thread,0);
		pthread_create(0,0,r_fft_thread,0);
		cerr<<"giving some time to FFT to prevent crash...\n";
		Sleep(1000);
		signal(SIGINT,on_sigint);
		init_audio();
		open_audio_device();
		audio_device_is_open=true;
		play_buffer(short_buf,bufsize>>1);
		while(get_pos()==0);
		unsigned long long t,s;
		string time_string,total_time_string,position_string,fps_string;
		unsigned long long delta_length;
		s=chrono::high_resolution_clock::now().time_since_epoch().count();
		for(unsigned long long i=0;i<num_slides;i++)
		{
			t=chrono::high_resolution_clock::now().time_since_epoch().count();
			if((t-s)/1000000.0>=i*window_size/48.0)//skip this frame if there's not enough time
				continue;
			clear_canvas();
			draw_bargraph(i);
			time_string=to_string(i/48000.0*window_size);
			total_time_string=to_string((bufsize>>1)/48000.0);
			delta_length=total_time_string.length()-time_string.length();
			for(unsigned long long j=0;j<delta_length;j++)
				time_string="  "+time_string;
			position_string=time_string+"/"+total_time_string;
			TextOut(canvas,420,550+window_border_shift,position_string.c_str(),position_string.length());
			t=chrono::high_resolution_clock::now().time_since_epoch().count();
			fps_buffer.insert(fps_buffer.begin(),t);
			if(fps_buffer.size()>20)
				fps_buffer.pop_back();
			fps_string="fps="+to_string(fps_buffer.size()*1000000000.0/(fps_buffer[0]-fps_buffer[fps_buffer.size()-1]));
			TextOut(canvas,880,4,fps_string.c_str(),fps_string.length());
			paint_to_window();
			if(main_restart_flag)
			{
				main_restart_flag=false;
				goto main_restart;
			}
			if(paused)
			{
				waveOutPause(out);
				while(paused);
				unsigned long long samples=get_pos();
				waveOutRestart(out);
				while(get_pos()==samples);
				s=chrono::high_resolution_clock::now().time_since_epoch().count()-i*window_size/48.0*1000000.0;
				continue;
			}
			if(main_seek_flag)
			{
				main_seek_flag=false;
				i=main_seek_frame-1;
				s=chrono::high_resolution_clock::now().time_since_epoch().count()-main_seek_frame*window_size/48.0*1000000.0;
				continue;
			}
			if((t-s)/1000000.0<i*window_size/48.0)
				Sleep(i*window_size/48.0-(t-s)/1000000.0);
		}
		delete[] short_buf;
	}
}
