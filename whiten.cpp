#include <iostream>
#include <vector>
#include <stdio.h>
#include "opencv2/core/core.hpp"
#include "opencv2/highgui/highgui.hpp"
#include "opencv2/contrib/contrib.hpp"
#include "opencv2/video/tracking.hpp"
#include "opencv2/imgproc/imgproc.hpp"
#include "fstream"
#include "iostream"
using namespace std;
using namespace cv;
const double E=2.7182818284590452353602874713527;
//----------------------------------------------------------
// ������� ��� ������������ ��������� ����������� ��������
// ���, ����� ���� ������� ��������� � ������ �����������.
//----------------------------------------------------------
void Recomb(Mat &src,Mat &dst)
{
    int cx = src.cols>>1;
    int cy = src.rows>>1;
	Mat tmp;
	tmp.create(src.size(),src.type());
	src(Rect(0, 0, cx, cy)).copyTo(tmp(Rect(cx, cy, cx, cy)));
	src(Rect(cx, cy, cx, cy)).copyTo(tmp(Rect(0, 0, cx, cy)));	
	src(Rect(cx, 0, cx, cy)).copyTo(tmp(Rect(0, cy, cx, cy)));
	src(Rect(0, cy, cx, cy)).copyTo(tmp(Rect(cx, 0, cx, cy)));
	dst=tmp;
}
//----------------------------------------------------------
// �� ��������� ����������� ������������ 
// �������������� � ������ ����� ������� �����
//----------------------------------------------------------
void ForwardFFT(Mat &Src, Mat *FImg)
{
	int M = getOptimalDFTSize( Src.rows );
    int N = getOptimalDFTSize( Src.cols );
	Mat padded;    
	copyMakeBorder(Src, padded, 0, M - Src.rows, 0, N - Src.cols, BORDER_CONSTANT, Scalar::all(0));
	// ������� ����������� ������������� �����������
	// planes[0] �������� ���� �����������, planes[1] ��� ������ ����� (��������� ������)
	Mat planes[] = {Mat_<float>(padded), Mat::zeros(padded.size(), CV_32F)};
    Mat complexImg;
    merge(planes, 2, complexImg); 
	dft(complexImg, complexImg);	
	// ����� �������������� ��������� ���-�� ������� �� �������������� � ������ �����
	split(complexImg, planes);

	// ������� ������, ���� � ���� �������� ���������� ����� ��� ��������
    planes[0] = planes[0](Rect(0, 0, planes[0].cols & -2, planes[0].rows & -2));
	planes[1] = planes[1](Rect(0, 0, planes[1].cols & -2, planes[1].rows & -2));

	Recomb(planes[0],planes[0]);
	Recomb(planes[1],planes[1]);
	// ����������� ������
	planes[0]/=float(M*N);
	planes[1]/=float(M*N);
	FImg[0]=planes[0].clone();
	FImg[1]=planes[1].clone();
}
//----------------------------------------------------------
// �� �������� �������������� � ������ �����
// ������� ����� ��������������� �����������
//----------------------------------------------------------
void InverseFFT(Mat *FImg,Mat &Dst)
{
	Recomb(FImg[0],FImg[0]);
	Recomb(FImg[1],FImg[1]);
	Mat complexImg;
	merge(FImg, 2, complexImg);
	// ���������� �������� �������������� �����
	idft(complexImg, complexImg);
	split(complexImg, FImg);
    normalize(FImg[0], Dst, 0, 1, CV_MINMAX);
}
//----------------------------------------------------------
// ������������ ����������� �� ��������� � ���� ������� �����
//----------------------------------------------------------
void ForwardFFT_Mag_Phase(Mat &src, Mat &Mag,Mat &Phase)
{
	Mat planes[2];
	ForwardFFT(src,planes);
	Mag.zeros(planes[0].rows,planes[0].cols,CV_32F);
	Phase.zeros(planes[0].rows,planes[0].cols,CV_32F);
	cv::cartToPolar(planes[0],planes[1],Mag,Phase);
}
//----------------------------------------------------------
// �� �������� ��������� � ����
// ������� ����� ��������������� �����������
//----------------------------------------------------------
void InverseFFT_Mag_Phase(Mat &Mag,Mat &Phase, Mat &dst)
{
	Mat planes[2];
	planes[0].create(Mag.rows,Mag.cols,CV_32F);
	planes[1].create(Mag.rows,Mag.cols,CV_32F);
	cv::polarToCart(Mag,Phase,planes[0],planes[1]);
	InverseFFT(planes,dst);
}
//----------------------------------------------------------
// Whiten image
//----------------------------------------------------------
void whiten(Mat& src,Mat &dst)
{
	double f0=200;
	//----------------------------------------------------------
	// ������� ��������� ������
	//----------------------------------------------------------
	Mat filter;
	filter=Mat::zeros(src.rows,src.cols,CV_32F);
	for(int i=0;i<src.rows;i++)
	{
		double I=(float)i-(float)src.rows/2.0;
		float* F = filter.ptr<float>(i);
		for(int j=0;j<src.cols;j++)
		{
			double J=(float)j-(float)src.cols/2.0;
			double f=sqrtl(I*I+J*J);
			F[j]=f*powf(E,-powf((f/f0),4.0));
		}
	}
	//----------------------------------------------------------
	// ���������� �������
	//----------------------------------------------------------
	// ������������ ����������� � ������
	// ��������� �������
	Mat Mag;
	// ���� �������
	Mat Phase;
	ForwardFFT_Mag_Phase(src,Mag,Phase);
	cv::multiply(Mag,filter,Mag);
	//cv::multiply(Phase,filter,Phase);
	//----------------------------------------------------------
	// �������� ��������������
	//----------------------------------------------------------
	InverseFFT_Mag_Phase(Mag,Phase,dst);
}
//-----------------------------------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------------------------------
int main( int argc, char** argv )
{
	namedWindow("Result");
	Mat I=imread("C:\\ImagesForTest\\lena.jpg",0);
	I.convertTo(I,CV_32FC1,1.0/255.0);
	Mat res;
	whiten(I,res);
	imshow("Result",res);
	waitKey(0);
	return 0;
}