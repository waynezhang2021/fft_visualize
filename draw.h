#pragma once
#include<windows.h>
#include<pthread.h>
using namespace std;
HDC console;
HDC canvas;
typedef struct
{
	int width;
	int height;
} size;
size s;
bool autodraw=false;
int drawfrequency=0;
double GetScalingFactor()
{
	HWND hWnd = GetDesktopWindow();
	HMONITOR hMonitor = MonitorFromWindow(hWnd, MONITOR_DEFAULTTONEAREST);
	MONITORINFOEX miex;
	miex.cbSize = sizeof(miex);
	GetMonitorInfo(hMonitor, &miex);
	int cxLogical = (miex.rcMonitor.right - miex.rcMonitor.left);
	//int cyLogical = (miex.rcMonitor.bottom - miex.rcMonitor.top);
	DEVMODE dm;
	dm.dmSize = sizeof(dm);
	dm.dmDriverExtra = 0;
	EnumDisplaySettings(miex.szDevice, ENUM_CURRENT_SETTINGS, &dm);
	int cxPhysical = dm.dmPelsWidth;
	//int cyPhysical = dm.dmPelsHeight;
	double horzScale = ((double)cxPhysical / (double)cxLogical);
	//double vertScale = ((double)cyPhysical / (double)cyLogical);
	return horzScale;
}
int GetRefreshRate()
{
	HWND hWnd = GetDesktopWindow();
	HMONITOR hMonitor = MonitorFromWindow(hWnd, MONITOR_DEFAULTTONEAREST);
	MONITORINFOEX miex;
	miex.cbSize = sizeof(miex);
	GetMonitorInfo(hMonitor, &miex);
	DEVMODE dm;
	dm.dmSize = sizeof(dm);
	dm.dmDriverExtra = 0;
	EnumDisplaySettings(miex.szDevice, ENUM_CURRENT_SETTINGS, &dm);
	return dm.dmDisplayFrequency;
}
size GetWindowSize(HWND hWnd)
{
	RECT rect;
	int width,height;
	double scaling_factor=GetScalingFactor();
	GetClientRect(hWnd,&rect);
	width=(rect.right-rect.left)*scaling_factor;
	height=(rect.bottom-rect.top)*scaling_factor;
	return {width,height};
}
inline void draw_pixel(int x,int y,COLORREF color)
{
	SetPixel(canvas,x,y,color);
}
inline void set_pen_color(COLORREF color)
{
	HPEN pen=CreatePen(PS_SOLID,1,color);
	SelectObject(canvas,pen);
}
inline void set_brush_color(COLORREF color)
{
	HBRUSH brush=CreateSolidBrush(color);
	SelectObject(canvas,brush);
}
inline void set_color(COLORREF color)
{
	set_pen_color(color);
	set_brush_color(color);
}
inline void set_color(COLORREF color_foreground,COLORREF color_background)
{
	set_pen_color(color_foreground);
	set_brush_color(color_background);
}
inline void ellipse(int x1,int y1,int x2,int y2,COLORREF color)
{
	set_color(color);
	Ellipse(canvas,x1,y1,x2,y2);
}
inline void ellipse(int x1,int y1,int x2,int y2,COLORREF color_foreground,COLORREF color_background)
{
	set_color(color_foreground,color_background);
	Ellipse(canvas,x1,y1,x2,y2);
}
inline void rectangle(int x1,int y1,int x2,int y2,COLORREF color)
{
	set_color(color);
	Rectangle(canvas,x1,y1,x2,y2);
}
inline void rectangle(int x1,int y1,int x2,int y2,COLORREF color_foreground,COLORREF color_background)
{
	set_color(color_foreground,color_background);
	Rectangle(canvas,x1,y1,x2,y2);
}
inline void paint_to_window(DWORD mode=SRCCOPY)
{
	BitBlt(console,0,0,s.width,s.height,canvas,0,0,mode);
}
inline void clear_canvas()
{
	BitBlt(canvas,0,0,s.width,s.height,canvas,0,0,SRCINVERT);
}
inline COLORREF rand_color_rgb()
{
	return RGB(rand()%256,rand()%256,rand()%256);
}
inline COLORREF rand_color_cmyk()
{
	return CMYK(rand()%256,rand()%256,rand()%256,rand()%256);
}
void* paint_thread(void*)
{
	while(1)
	{
		if(autodraw)
		{
			Sleep(1000.0/drawfrequency);
			paint_to_window();
		}
		else
			Sleep(5);
	}
	return nullptr;
}
void move_window(int x,int y)
{
	HWND window=GetConsoleWindow();
	RECT rect;
	GetWindowRect(window,&rect); 
	MoveWindow(window,x,y,rect.right-rect.left,rect.bottom-rect.top,true);
}
void resize_window(int x,int y)
{
	HWND window=GetConsoleWindow();
	RECT rect;
	GetWindowRect(window,&rect); 
	MoveWindow(window,rect.left,rect.top,x,y,true);
}
void init_graphics()
{
	srand(time(0)+clock());
	CONSOLE_CURSOR_INFO cci={10,false};
	SetConsoleCursorInfo(GetStdHandle(STD_OUTPUT_HANDLE),&cci);
	HWND window=GetConsoleWindow();
	console=GetDC(window);
	canvas=CreateCompatibleDC(console);
	s=GetWindowSize(window);
	HBITMAP bitmap;
	bitmap=CreateCompatibleBitmap(console,s.width,s.height);
	SelectObject(canvas,bitmap);
	drawfrequency=GetRefreshRate();
	pthread_create(0,0,paint_thread,0);
}
void auto_draw(bool enabled=false,int frequency=0)
{
	if(enabled==false)
		autodraw=false;
	else
	{
		autodraw=true;
		if(frequency==0)
			drawfrequency=GetRefreshRate();
		else
			drawfrequency=frequency;
	}
}
