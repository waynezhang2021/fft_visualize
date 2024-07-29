#include<windows.h>
#include<string>
//everything here is based on signed 16-bit little-endian 48000-Hz stereo PCM format
//in ffmpeg:-f s16le -ar 48000 -ac 2
HWAVEOUT out;
WAVEFORMATEX waveform;
WAVEHDR header;
unsigned long long get_file_size(string filename)
{
	HANDLE hfile=CreateFile(filename.c_str(),GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
	LARGE_INTEGER l_file_size;
	GetFileSizeEx(hfile,&l_file_size);
	CloseHandle(hfile);
	return l_file_size.QuadPart;
}
unsigned long long get_sample_count(string filename)
{
	unsigned long long file_size=get_file_size(filename);
	if(file_size&3)
		throw "file size is not a multiple of 4\nnot a valid 16-bit stereo PCM file\n";
	else
		return file_size>>2;
}
void init_audio()
{
	waveform.wFormatTag=WAVE_FORMAT_PCM;
	waveform.nSamplesPerSec=48000;
	waveform.wBitsPerSample=16;
	waveform.nChannels=2;
	waveform.nAvgBytesPerSec=4*48000;
	waveform.nBlockAlign=4;
	waveform.cbSize=0;
}
void open_audio_device()
{
	waveOutOpen(&out,WAVE_MAPPER,&waveform,(DWORD_PTR)nullptr,0,CALLBACK_NULL);
}
void play_buffer(short* buf,unsigned long long bufsize)
{
	header.lpData=(LPSTR)buf;
	header.dwBufferLength=bufsize*4;
	header.dwBytesRecorded=0;
	header.dwUser=0;
	header.dwFlags=WAVE_ALLOWSYNC;
	header.dwLoops=1;
	waveOutPrepareHeader(out,&header,sizeof(WAVEHDR));
	waveOutWrite(out,&header,sizeof(WAVEHDR));
	waveOutUnprepareHeader(out,&header,sizeof(WAVEHDR));
}
void close_audio_device()
{
	waveOutClose(out);
}
