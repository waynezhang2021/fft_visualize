#include<math.h>
#include<vector>
#include<string>
#include<complex>
#include<iostream>
using namespace std;
complex<long double> j(0,1);
long double ld_eps=1e-9;
long double pi=3.14159265358979323846264338327950288419716939937;
unsigned long long lookup_table_size=0;
long double lookup_table_sign=0;
vector<complex<long double>> exp_lookup_table;
vector<complex<long double>> internal_fft(vector<complex<long double>> data,long double sign)
{
	unsigned long long size=data.size(),half_size=size>>1;
	unsigned long long size_scaling_factor=lookup_table_size/size;
	if(size<=1)
		return data;
	vector<complex<long double>> even,odd;
	for(unsigned long long i=0;i<size;i++)
	{
		if(i&1)
			odd.push_back(data[i]);
		else
			even.push_back(data[i]);
	}
	vector<complex<long double>> even_fft=internal_fft(even,sign),odd_fft=internal_fft(odd,sign);
	vector<complex<long double>> result;
	for(unsigned long long i=0;i<half_size;i++)
		result.push_back((exp_lookup_table[i*size_scaling_factor]*odd_fft[i]+even_fft[i]));
	for(unsigned long long i=half_size;i<size;i++)
		result.push_back((exp_lookup_table[i*size_scaling_factor]*odd_fft[i-half_size]+even_fft[i-half_size]));
	return result;
}
inline void make_lookup_table(unsigned long long size,long double sign)
{
	if(lookup_table_size==size&&lookup_table_sign==sign)
		return;
	exp_lookup_table.clear();
	lookup_table_size=size;
	lookup_table_sign=sign;
	long double pi_div_by_size=pi/size;
	for(unsigned long long i=0;i<size;i++)
		exp_lookup_table.push_back(exp(-2.0l*sign*j*(i*pi_div_by_size)));
}
inline vector<complex<long double>> fft(vector<complex<long double>> data)
{
	if(int(log2(data.size()))!=log2(data.size()))
		throw "size of input is not exponent of 2";
	make_lookup_table(data.size(),1);
	vector<complex<long double>> r=internal_fft(data,1);
	for(unsigned long long i=0;i<r.size();i++)
		r[i]/=(long double)r.size();
	return r;
}
inline vector<complex<long double>> ifft(vector<complex<long double>> data)
{
	if(int(log2(data.size()))!=log2(data.size()))
		throw "size of input is not exponent of 2";
	make_lookup_table(data.size(),-1);
	return internal_fft(data,-1);
}
string format_ld(long double ld)
{
	string s=to_string(ld);
	unsigned long long index=s.length()-1;
	while(1)
	{
		if(s[index]!='0'&&s[index]!='.')
			break;
		index--;
	}
	return s.substr(0,index+1);
}
string format_complex(complex<long double> c)
{
	if(abs(c.real())<ld_eps&&abs(c.imag())<ld_eps)
		return "0";
	if(abs(c.real())<ld_eps&&abs(c.imag())>=ld_eps)
		return format_ld(c.imag())+"i";
	if(abs(c.real())>=ld_eps&&abs(c.imag())<ld_eps)
		return format_ld(c.real());
	if(abs(c.real())>=ld_eps&&abs(c.imag())>=ld_eps)
	{
		if(c.imag()>0)
			return format_ld(c.real())+"+"+format_ld(c.imag())+"i";
		else
			return format_ld(c.real())+format_ld(c.imag())+"i";
	}
	if(c.imag()>0)
		return format_ld(c.real())+"+"+format_ld(c.imag())+"i";
	else
		return format_ld(c.real())+format_ld(c.imag())+"i";
}
ostream& operator<<(ostream& o,vector<complex<long double>> v)
{
	cout<<"{";
	for(unsigned long long i=0;i<v.size()-1;i++)
		cout<<format_complex(v[i])<<",";
	cout<<format_complex(v[v.size()-1]);
	cout<<"}";
	return o;
}
