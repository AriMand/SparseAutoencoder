#include <iostream>
#include <vector>
#include <stdio.h>
//#include <Eigen/Dense>
#include "opencv2/core/core.hpp"
#include "opencv2/gpu/gpu.hpp"
#include "opencv2/ml/ml.hpp"
#include "opencv2/highgui/highgui.hpp"
#include "opencv2/contrib/contrib.hpp"
#include "opencv2/video/tracking.hpp"
#include "opencv2/imgproc/imgproc.hpp"
//#include "opencv2/core/eigen.hpp"
#include "fstream"
#include "iostream"
#include <time.h>
#include <stdlib.h>
#include <lbfgs.h>
using namespace std;
using namespace cv;
//using namespace Eigen;
const double E=2.7182818284590452353602874713527;
// ��� ���� �����
Mat Patches;
//-----------------------------------------------------------------------------------------------------
// ������� (������������) �������
//-----------------------------------------------------------------------------------------------------
Mat sigmoid(Mat& x)
{
	Mat res(x.rows,x.cols,CV_64FC1);
	exp(-x,res);
	res=1.0/(res+1.0);
	return res;
}
/*
//-----------------------------------------------------------------------------------------------------
// ����� ����� �� ��������� [0,1) �������� ������������ �������������
//-----------------------------------------------------------------------------------------------------
double Rand(void)
{
	return double(rand())/double(RAND_MAX);
}
*/
//-----------------------------------------------------------------------------------------------------
// ������� ������ � ���������
//-----------------------------------------------------------------------------------------------------
void sparseAutoencoderCost(Mat &W,int visibleSize,int hiddenSize,double lambda,double sparsityParam,double beta, Mat& data,double* cost,Mat& grad)
{
	// visibleSize: ���������� ������� ����� (probably 64)
	// hiddenSize: ���������� ������� ����� (probably 25)
	// lambda: ����������� ���������� �����
	// sparsityParam: �������� ������� ��������� �������� �������� ���� (��).
	// beta: ����������� (���) ���������� ����������� �� �������������.
	// data: ���� ������� 64x10000 ���������� ��������� �������.
	// ����� �������, data(:,i) ��� i-th ��������� ���� (���� � �����, � ������ ������ ���� � ��-��).
	//
	// ������� �������� W ��� ������ (�.�. minFunc �������, ��� �������� �������� ��������).
	// ������� �� �������� W �� ����� (W1, W2, b1, b2), ����� ��� ���� ��� � ������.

	// ����, ����������� ������� ������ � ������� ����
	Mat W1=W(Range(0,hiddenSize*visibleSize),Range::all()).clone();
	W1 = W1.reshape(1,hiddenSize);
	// ����, ����������� ������� ���� � �����
	Mat W2=W(Range(hiddenSize*visibleSize,2*hiddenSize*visibleSize),Range::all()).clone();
	W2 = W2.reshape(1,visibleSize);
	// �������� �������� �������� ����
	Mat b1=W(Range(2*hiddenSize*visibleSize,2*hiddenSize*visibleSize+hiddenSize),Range::all()).clone();
	// �������� �������� ��������� ����
	Mat b2=W(Range(2*hiddenSize*visibleSize+hiddenSize,W.rows),Range::all()).clone();

	// ������� ��������� � ��������� (��� ��� ������ ���������� ��� ��������).
	// ��� ��� ���������������� ������.
	Mat W1grad = Mat::zeros(W1.rows,W1.cols,CV_64FC1);
	Mat W2grad = Mat::zeros(W2.rows,W2.cols,CV_64FC1);
	Mat b1grad =  Mat::zeros(b1.rows,b1.cols,CV_64FC1);
	Mat b2grad =  Mat::zeros(b2.rows,b2.cols,CV_64FC1);

	double numPatches=data.cols;

	Mat avgActivations=Mat::zeros(W1.rows,1,CV_64FC1);
	static Mat storedHiddenValues = Mat::zeros(hiddenSize, numPatches,CV_64FC1);
	static Mat storedOutputValues = Mat::zeros(visibleSize, numPatches,CV_64FC1);

	double J=0;
	static Mat X,z2,z3,a2,a3,tmp;
	//----------------------------
	// ������ ������ (������ ������ ����)
	//----------------------------
	for (int i=0;i<numPatches;i++)
	{
		data.col(i).copyTo(X);
		z2=W1*X+b1;
		a2=sigmoid(z2);
		avgActivations=avgActivations+a2;
		z3=W2*a2+b2;
		a3=sigmoid(z3);
		// �������� ���������� ������� ����
		a2.copyTo(storedHiddenValues.col(i));
		a3.copyTo(storedOutputValues.col(i));
		// ��������� ������� ������ (����� ��������� ������)
		tmp=a3-X;
		pow(tmp,2,tmp);
		J=J+0.5*sum(tmp)[0];
	}
	//----------------------------
	// ����������, ��������� � �������� �������������
	// �� ���� ����� � ������� ���� �� �������� ������
	// �������������� ���� ��������� ���������� ��������
	//----------------------------

	// �� ��������� ����� ������ �������
	avgActivations=avgActivations/numPatches;

	// ����������� � ������ �������� ���� ��� �������� �������
	Mat sparsity_grad=beta*(-(sparsityParam/avgActivations)+((1.0-sparsityParam)/(1.0-avgActivations)));

	// ��������� ����������� ��������-��������
	//Mat tmp1;
	log(sparsityParam/avgActivations,tmp);
	Mat KL1=sparsityParam*tmp;
	log((1.0-sparsityParam)/(1.0-avgActivations),tmp);
	Mat KL2=(1.0-sparsityParam)*tmp;
	// ����������� ��������-�������� (����� ��������� �� ���� ������� ������)
	double KL_divergence=sum(KL1+KL2)[0];
	// ������� ������ (�������������� ����������)

	static Mat W1Sqr;
	cv::pow(W1,2.0,W1Sqr);
	static Mat W2Sqr;
	cv::pow(W2,2.0,W2Sqr);
	(*cost)=(J/numPatches)+lambda*0.5*(sum(W1Sqr)[0]+sum(W2Sqr)[0])+beta*KL_divergence;
	//----------------------------
	// �������� ��������������� ������
	//----------------------------

	for (int i=0;i<numPatches;i++)
	{
		data.col(i).copyTo(X);
		// ������� ����� ����������� ����
		storedHiddenValues.col(i).copyTo(a2);
		storedOutputValues.col(i).copyTo(a3);

		// ������ ��������� ����
		Mat delta_3=(a3-X).mul(a3.mul(1.0-a3));
		// ������ �������� ����
		Mat delta_2=(W2.t()*delta_3+sparsity_grad).mul(a2.mul(1.0-a2));

		W1grad+=delta_2*X.t();
		W2grad+=delta_3*a2.t();

		b1grad+=delta_2;
		b2grad+=delta_3;
	}

	//----------------------------
	// ��������� �����
	//----------------------------
	W1grad=W1grad/numPatches+(lambda)*W1;
	W2grad=W2grad/numPatches+(lambda)*W2;
	//----------------------------
	// ��������� ��������
	//----------------------------
	b1grad = b1grad/numPatches;
	b2grad = b2grad/numPatches;
	//----------------------------
	// ������� ����������� ��������
	// ���������� � ������-�������
	// (���������� ��� minFunc).
	//----------------------------

	W1grad=W1grad.reshape(1,hiddenSize*visibleSize);
	W2grad=W2grad.reshape(1,hiddenSize*visibleSize);
	b1grad=b1grad.reshape(1,hiddenSize);
	b2grad=b2grad.reshape(1,visibleSize);

	grad=Mat(W1grad.rows+W2grad.rows+b1grad.rows+b2grad.rows,1,CV_64FC1);
	W1grad.copyTo(grad(Range(0,hiddenSize*visibleSize),Range::all()));
	W2grad.copyTo(grad(Range(hiddenSize*visibleSize,2*hiddenSize*visibleSize),Range::all()));
	b1grad.copyTo(grad(Range(2*hiddenSize*visibleSize,2*hiddenSize*visibleSize+hiddenSize),Range::all()));
	b2grad.copyTo(grad(Range(2*hiddenSize*visibleSize+hiddenSize,grad.rows),Range::all()));
}
//------------------------------------------------------------
// ������� ������������� ����������
//------------------------------------------------------------
Mat initializeParameters(int hiddenSize,int visibleSize)
{
	Mat theta;
	// �������������� ���� ������ ���������� ����������, ����������� �� �������� ����.
	double  r = sqrt(6.0 / ((double)hiddenSize+(double)visibleSize+1.0));
	// ������ ������� �� ��������� [-r, r] �� ������������ ������ �������������
	//W1 = rand(hiddenSize, visibleSize) * 2 * r - r;
	Mat W1=Mat(hiddenSize,visibleSize,CV_64FC1);
	cv::randu(W1,-r,r);
	Mat W2=Mat(visibleSize,hiddenSize,CV_64FC1);
	cv::randu(W2,-r,r);
	//W2 = rand(visibleSize, hiddenSize) * 2 * r - r;
	// �������� �������� ������
	Mat b1 = Mat::zeros(hiddenSize, 1,CV_64FC1);
	Mat b2 = Mat::zeros(visibleSize, 1,CV_64FC1);
	// ��������� ���� � �������� � ����� �������-�������.
	// ���� ��� "�������������" ��� ��������� (���� � ��������) � ���� ������  
	// ������� ����� �������������� �������� minFunc. 

	//theta = [W1(:) ; W2(:) ; b1(:) ; b2(:)];
	W1=W1.reshape(1,hiddenSize*visibleSize);
	W2=W2.reshape(1,hiddenSize*visibleSize);
	b1=b1.reshape(1,hiddenSize);
	b2=b2.reshape(1,visibleSize);
	theta=Mat(W1.rows+W2.rows+b1.rows+b2.rows,1,CV_64FC1);
	W1.copyTo(theta(Range(0,hiddenSize*visibleSize),Range::all()));
	W2.copyTo(theta(Range(hiddenSize*visibleSize,2*hiddenSize*visibleSize),Range::all()));
	b1.copyTo(theta(Range(2*hiddenSize*visibleSize,2*hiddenSize*visibleSize+hiddenSize),Range::all()));
	b2.copyTo(theta(Range(2*hiddenSize*visibleSize+hiddenSize,theta.rows),Range::all()));

	return theta;
}
//-----------------------------------------------------------------------------------------------------
// ��������� ���� (�������� ����)
//-----------------------------------------------------------------------------------------------------
void DrawNet(Mat& net,Mat& patches_img,int hiddenSize,int visibleSize,int patch_side,int scale)
{
	// ��������� N_GridRows*N_GridCols ������ ������
	int N_GridRows=sqrtf(hiddenSize);
	int N_GridCols=N_GridRows;

	patches_img=Mat((patch_side+1)*N_GridRows*scale,(patch_side+1)*N_GridCols*scale,CV_64FC1);
	patches_img=0;
	Mat p=(net(Range(0,visibleSize*hiddenSize),Range::all())).t();

	p=p.reshape(1,hiddenSize).clone();
	normalize(p,p,0,1,cv::NORM_MINMAX); // �������� � �������� ���������

	for(int i=0;i<N_GridRows*N_GridCols;i++)
	{	
		Mat p_im;
		p.row(i).copyTo(p_im);
		p_im=p_im.reshape(1,patch_side);

//		normalize(p_im,p_im,0,1,cv::NORM_MINMAX); // �������� � �������� ���������

		for(int j=0;j<p_im.rows;j++)
		{
			for(int k=0;k<p_im.cols;k++)
			{
				Point pt1=Point((i/N_GridCols)*(patch_side+1)*scale+k*scale+0.5*scale,(i%N_GridCols)*(patch_side+1)*scale+j*scale+0.5*scale);
				Point pt2=Point(((i/N_GridCols)*(patch_side+1)+1)*scale+k*scale+0.5*scale,((i%N_GridCols)*(patch_side+1)+1)*scale+j*scale+0.5*scale);
				rectangle(patches_img,pt1,pt2,Scalar::all(p_im.at<double>(j,k)),-1);
			}
		}
	}
}

//-----------------------------------------------------------------------------------------------------
// ��������� ����
//-----------------------------------------------------------------------------------------------------
int patch_side=8;
int visibleSize=patch_side*patch_side;
int hiddenSize=16;
Mat theta;
double lambda=0.0001;
double sparsityParam=0.04;
double beta=3;

//-----------------------------------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------------------------------
static lbfgsfloatval_t evaluate(
	void *instance,
	const lbfgsfloatval_t *x, // ������ ����������
	lbfgsfloatval_t *g,		  // ������ ����������
	const int n,			  // ����������� �������
	const lbfgsfloatval_t step
	)
{

	// *x - ��������� ����� theta.data, ������� ����������� �� ���������

	Mat grad;// ��������
	lbfgsfloatval_t fx = 0.0; // ������� (��������������) �������

	sparseAutoencoderCost(theta,visibleSize,hiddenSize,lambda,sparsityParam,beta, Patches,&fx,grad);

	// �������� ��������
	memcpy(g,grad.data,n *sizeof(double));

	return fx;
}
//-----------------------------------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------------------------------
static int progress(
	void *instance,
	const lbfgsfloatval_t *x,
	const lbfgsfloatval_t *g,
	const lbfgsfloatval_t fx,
	const lbfgsfloatval_t xnorm,
	const lbfgsfloatval_t gnorm,
	const lbfgsfloatval_t step,
	int n,
	int k,
	int ls
	)
{
	// *x - ��������� ����� theta.data, ������� ����������� �� ���������
	// ������ ���
	Mat patches_img;
	int scale=5; // ������� ����������
	DrawNet(theta,patches_img,hiddenSize,visibleSize,patch_side,scale);
	imshow("Patches",patches_img);
	waitKey(15);

	printf("Iteration %d:\n", k);
	printf("  fx = %f, x[0] = %f, x[1] = %f\n", fx, x[0], x[1]);
	printf("  xnorm = %f, gnorm = %f, step = %f\n", xnorm, gnorm, step);
	printf("\n");
	return 0;
}

//-----------------------------------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------------------------------
int main( int argc, char** argv )
{
	// ----------------------------------------------
	// ������������� ���������� ��������� �����
	// ----------------------------------------------
	srand((unsigned)time(NULL));
	
	// ����� ����� ����������� � ������� ����� ����� �����
	vector<Mat> Images;
	namedWindow("Image");
	namedWindow("Patches");
	// ----------------------------------------------
	// ������ �����������, � ������� ����� ����� �����
	// ----------------------------------------------
	for(int i=0;i<10;i++)
	{
		stringstream str;
		str << "..\\images\\image"<<i<<".jpg";
		Mat img;
		imread(str.str(),0).convertTo(img,CV_64FC1,1.0/255.0);
		Images.push_back(img);
		cout << str.str() << endl;
	}
	// ----------------------------------------------
	// ������ �������� �����
	// ----------------------------------------------
	double n_patches=10000;
	Patches=Mat(patch_side*patch_side,n_patches,CV_64FC1);
	RNG rng;
	for(int i=0;i<n_patches;i++)
	{
		int n_img=rng.uniform(0.0,1.0)*((double)Images.size()-1);
		int row=rng.uniform(0.0,1.0)*((double)Images[n_img].rows-patch_side);
		int col=rng.uniform(0.0,1.0)*((double)Images[n_img].cols-patch_side);
		Mat patch=Images[n_img](Range(row,row+patch_side),Range(col,col+patch_side)).clone();
		patch=patch.reshape(1,patch_side*patch_side).clone();
		patch.copyTo(Patches.col(i));
	}
	// -------------------------------------------------------------------------------
	// ������������ 
	// (����������� �� ������,
	// �������� �������,
	// �������� � ��������� 0.1-0.9, �.� �� ������ � ��� ���������� ������� ���������)
	// -------------------------------------------------------------------------------
	Scalar mean;
	Scalar stddev;
	cv::meanStdDev(Patches,mean,stddev);
	//sqrt(stddev,stddev);
	Patches-=mean;
	for(int i=0;i<Patches.rows;i++)
	{
		for(int j=0;j<Patches.cols;j++)
		{
			if(Patches.at<double>(i,j)>3.0*stddev[0]){Patches.at<double>(i,j)=3.0*stddev[0];}
			if(Patches.at<double>(i,j)<-3.0*stddev[0]){Patches.at<double>(i,j)=-3.0*stddev[0];}
		}
	}
	normalize(Patches,Patches,0.1,0.9,cv::NORM_MINMAX); // �������� � �������� ���������
	// -------------------------------------------------------------------------------
	// ������������� ������� ���������� ���������� ���������� ����������
	// -------------------------------------------------------------------------------
	theta=initializeParameters(hiddenSize,visibleSize);

	int ret = 0;
	lbfgsfloatval_t fx;
	lbfgsfloatval_t *x = lbfgs_malloc(theta.rows);
	lbfgs_parameter_t param;
	// Initialize the parameters for the L-BFGS optimization. 
	lbfgs_parameter_init(&param);
	//
	// Start the L-BFGS optimization; this will invoke the callback functions
	// evaluate() and progress() when necessary.
	//
	ret = lbfgs(theta.rows,(lbfgsfloatval_t *) theta.data, &fx, evaluate, progress, NULL, &param);

	// Report the result.
	printf("L-BFGS optimization terminated with status code = %d\n", ret);
	printf("  fx = %f, x[0] = %f, x[1] = %f\n", fx, x[0], x[1]);

	lbfgs_free(x);

	waitKey(0);

	return 0;
}