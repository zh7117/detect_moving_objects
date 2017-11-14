#include "stdio.h"
#include "cxcore.h"
#include "cv.h"
#include "highgui.h"
# include <iostream>
#include <math.h>
#include <opencv.hpp>
#include "tinyxml.h"
#include "string"
#include "strstream"
using namespace std;
# define NULL 0
# define tresh 500
# define distance 50
# define path "zh.mp4"
# define savepath "zh.xml"
typedef struct objectList
{
	int x;
	int y;
	struct objectList *next=NULL;
};
typedef struct object {
	int state=0;
	int index;
	int x;
	int y;
	CvPoint points[2];
	int area;
	struct objectList* head;
	struct object* next=NULL;
};
const int winHeight = 480;
const int winWidth = 640;
static int nFrmNum = 0;
static int label_num = 0;
static CvMat*imagegrayMat;
static CvMat*backMat;
static CvMat*foreMat;
static CvMat*lableMat;
static IplImage*image;
static IplImage*imagegray;
static IplImage*back;
static IplImage*fore;
static int image_width;
static int image_height;
static int* mask;
static struct object* preobjects=new object();
static struct object* currobjects=new object();
static CvFont font1, font2;
static char buf[10];
void FillInternalContours(IplImage *pBinary, double dAreaThre)//去除前景中面积小于dAreaThre的连通域，填补大于dAreaThre的连通域的中空现象
{
	CvSeq *pContour = NULL;
	CvMemStorage *pStorage = NULL;  
	if (pBinary)
	{   
		pStorage = cvCreateMemStorage(0);
		//计算前景中所有连通域的外层轮廓
		cvFindContours(pBinary, pStorage, &pContour, sizeof(CvContour), CV_RETR_EXTERNAL, CV_CHAIN_APPROX_SIMPLE);   	
		//std::cout << CV_FILLED << "\n";   
		int wai = 0;
		for (; pContour != NULL; pContour = pContour->h_next)
		{
			wai++;
			if (fabs(cvContourArea(pContour, CV_WHOLE_SEQ)) < dAreaThre) {
				//填充为黑色
				cvDrawContours(pBinary, pContour, CV_RGB(0, 0, 0), CV_RGB(0, 0, 0), 2, CV_FILLED, 8, cvPoint(0, 0));
			}
			else {
				//填充为白色
				cvDrawContours(pBinary, pContour, CV_RGB(255, 255, 255), CV_RGB(255, 255, 255), 2, CV_FILLED, 8, cvPoint(0, 0));
		}
		}
		cvReleaseMemStorage(&pStorage);
		pStorage = NULL;
		cvDilate(fore,fore,0,1);
		cvErode(fore,fore,0,1);
	}
}
/*int Otsu(IplImage* src)//binary picture
{
	int height = src->height;
	int width = src->width;

	//histogram        
	float histogram[256] = { 1 };
	for (int i = 0; i < height; i++)
	{
		unsigned char* p = (unsigned char*)src->imageData + src->widthStep * i;
		//std::cout<<i
		for (int j = 0; j < width; j++)
		{
			histogram[*p++]++;
		}
	}
	//normalize histogram        
	int size = height * width;
	for (int i = 0; i < 256; i++)
	{
		histogram[i] = histogram[i] / size;
	}

	//average pixel value        
	float avgValue = 0;
	for (int i = 0; i < 256; i++)
	{
		avgValue += i * histogram[i];  //整幅图像的平均灰度      
	}

	int threshold;
	float maxVariance = 0;
	float w = 0, u = 0;
	for (int i = 0; i < 256; i++)
	{
		w += histogram[i];  //假设当前灰度i为阈值, 0~i 灰度的像素(假设像素值在此范围的像素叫做前景像素) 所占整幅图像的比例      
		u += i * histogram[i];  // 灰度i 之前的像素(0~i)的平均灰度值： 前景像素的平均灰度值      

		float t = avgValue * w - u;
		float variance = t * t / (w * (1 - w));
		if (variance > maxVariance)
		{
			maxVariance = variance;
			threshold = i;
		}
	}

	return threshold;
}*/
/*void myRunningAvg(CvMat* srcMat, CvMat* dstMat, int* maskFrame, movingObject* m_ObjectInfo, float alpha, int n)
{
	int i, j, k;
	int height = srcMat->rows;
	int width = srcMat->cols;
	memset(maskFrame, 0, sizeof(int)*(height*width));
	for (i = n; i>0; i--)
	{
		for (k = m_ObjectInfo[i].points[0].y; k <= m_ObjectInfo[i].points[1].y; k++)
			for (j = m_ObjectInfo[i].points[0].x; j <= m_ObjectInfo[i].points[1].x; j++)
				maskFrame[k*width + j] = 1;
	}
	for (j = 0; j<height; j++)
		for (i = 0; i<width; i++)
		{
			if (maskFrame[j*width + i] == 0)
			{
				dstMat->data.fl[j * width + i] = (srcMat->data.fl[j * width + i])*alpha + (1 - alpha)*dstMat->data.fl[j * width + i];
			}
			else
				dstMat->data.fl[j * width + i] = dstMat->data.fl[j * width + i];
		}
}*/
void testRunningAvg(float alpha)
{
	int i, j, k;
	int height = image_height;
	int width = image_width;
	for (j = 0; j<image_height; j++)
		for (i = 0; i<image_width; i++)
		{
			if (fore->imageData[j*width+i] == 0)
			{
				backMat->data.fl[j * width + i] = (imagegrayMat->data.fl[j * width + i])*alpha + (1 - alpha)*backMat->data.fl[j * width + i];
			}
			else
				backMat->data.fl[j * width + i] = backMat->data.fl[j * width + i];
		}
}
void computeback() {
	nFrmNum++;
	if (nFrmNum == 1)
	{
		//cvCvtColor(image, back, CV_BGR2GRAY);
		cvConvert(back, backMat);
	}
	else
	{
		//cvRunningAvg(imagegrayMat,backMat,0.01,0);
		testRunningAvg(0.01);
		cvConvert(backMat,back);
	}
}

void computefore() {
	//cvSmooth(imageMat, imageMat, CV_GAUSSIAN, 3, 0, 0, 0);
	/*if (nFrmNum == 1) {
		cvCvtColor(image, fore, CV_BGR2GRAY);
		cvConvert(fore, foreMat);
	}*/
	//else {
		cvAbsDiff(imagegrayMat, backMat, foreMat);
		cvThreshold(foreMat, fore, 40, 255, CV_THRESH_BINARY);
		cvConvert(fore, foreMat);
	//}
	//cvErode(fore,fore,0,1);
	cvDilate(fore,fore,0,1);
}
int calc_distance(int x1, int y1, int x2, int y2) {
	return (int)sqrt((x1 - x2)*(x1 - x2) + (y1 - y2)*(y1 - y2));
}
void computeObjects() {
	if (nFrmNum == 1) {
		return;
	}
	CvSeq *pContour = NULL;
	CvMemStorage *pStorage = NULL;   
	object*p = currobjects;
	p->next = NULL;
	p->head = NULL;
	IplImage *bin = cvCreateImage(cvGetSize(fore), 8, 1);
	cvThreshold(fore, bin, 50, 255, CV_THRESH_BINARY);
	if (fore)
	{  
		pStorage = cvCreateMemStorage(0);
		cvFindContours(bin, pStorage, &pContour, sizeof(CvContour), CV_RETR_EXTERNAL, CV_CHAIN_APPROX_SIMPLE);
		for (; pContour != NULL; pContour = pContour->h_next) {
				if (nFrmNum == 2) {
					CvRect rect = cvBoundingRect(pContour, 0);
					object*t = new object();
					t->area = fabs(cvContourArea(pContour, CV_WHOLE_SEQ));
					t->index = ++label_num;
					t->x = rect.x + rect.width / 2.0;
					t->y = rect.y + rect.height / 2.0;
					t->points[0] = cvPoint(rect.x, rect.y);
					t->points[1] = cvPoint(rect.x + rect.width, rect.y + rect.height);
					t->next = NULL;
					t->head = new objectList();
					t->head->x = t->x;
					t->head->y = t->y;
					t->head->next = NULL;
					p->next = t;
					p = p->next;
				}
				else {
					CvRect rect = cvBoundingRect(pContour, 0);
					object*t = new object();
					t->area = fabs(cvContourArea(pContour, CV_WHOLE_SEQ));
					t->x = rect.x + rect.width / 2.0;
					t->y = rect.y + rect.height / 2.0;
					t->points[0] = cvPoint(rect.x, rect.y);
					t->points[1] = cvPoint(rect.x + rect.width, rect.y + rect.height);
					t->next = NULL;
					t->head = new objectList();
					t->head->x = t->x;
					t->head->y = t->y;
					t->head->next = NULL;
					object*last = preobjects->next;
					for (;last;last = last->next) {
						if (calc_distance(last->x, last->y, t->x, t->y) < distance) {
							t->index = last->index;
							t->head->next = last->head;
							last->head = NULL;
							break;
						}
					}
					if (last == NULL) {
						t->index = ++label_num;
					}
					p->next = t;
					p = p->next;
					last = NULL;
				}
			
			
		}
		cvReleaseMemStorage(&pStorage);
		pStorage = NULL;
	}
	preobjects->next = currobjects->next;
	
}

void drawObjects() {
	object* p = currobjects->next;
	while (p) {
		char buf[10];
		sprintf_s(buf, 10, "%d", p->index);
		cvRectangle(image, p->points[0], p->points[1], CV_RGB(255, 255, 255), 1, 8, 0);
		cvPutText(image, buf, cvPoint(p->points[0].x, p->points[0].y + 20), &font1, CV_RGB(255, 255, 0));
		p = p->next;
	}
	currobjects->next = NULL;
}

int main() {
	TiXmlDocument *myDocument = new TiXmlDocument();
	TiXmlDeclaration *declaration = new TiXmlDeclaration("1.0", "UTF-8", "");
	myDocument->LinkEndChild(declaration);
	TiXmlElement *RootElement = new TiXmlElement("object_tracking");
	myDocument->LinkEndChild(RootElement);
	//////////
	cvInitFont(&font1, CV_FONT_HERSHEY_TRIPLEX, 1, 1);
	cvInitFont(&font2, CV_FONT_HERSHEY_COMPLEX, 0.5, 0.5);
	CvCapture* capture = cvCaptureFromFile(path);
	IplImage*frame = cvQueryFrame(capture);
	image_width = frame->width;
	image_height = frame->height;
	image = cvCreateImage(cvSize(image_width, image_height), IPL_DEPTH_8U, 3);
	back = cvCreateImage(cvSize(image_width, image_height), IPL_DEPTH_8U, 1);
	fore = cvCreateImage(cvSize(image_width, image_height), IPL_DEPTH_8U, 1);
	imagegray = cvCreateImage(cvSize(image_width, image_height), IPL_DEPTH_8U, 1);
	backMat = cvCreateMat(image_height, image_width, CV_32FC1);
	foreMat = cvCreateMat(image_height, image_width, CV_32FC1);
	imagegrayMat = cvCreateMat(image_height, image_width, CV_32FC1);
	lableMat= cvCreateMat(image_height, image_width, CV_32FC1);
	mask =new int [image_width*image_height];
	////////////////////////////////////////////////////
	cvZero(backMat);
	while (true) {
		nFrmNum++;
		frame = cvQueryFrame(capture);
		if (frame == NULL) {
			for (int j = 0;j<image_height;j++)
				for (int i = 0;i < image_width;i++) {
					backMat->data.fl[j * image_width + i] = (backMat->data.fl[j * image_width + i])/ (nFrmNum);
				}
			nFrmNum = 0;
			break;
		}
		cvCvtColor(frame, imagegray, CV_BGR2GRAY);
		cvConvert(imagegray,imagegrayMat);
		for(int j=0;j<image_height;j++)
			for (int i = 0;i < image_width;i++) {
				backMat->data.fl[j * image_width + i] +=imagegrayMat->data.fl[j * image_width + i];
			}
	}
	cvConvert(backMat, back);
	capture = cvCaptureFromFile(path);
	//////////////////////////////////////////////////////////////
	while (1) {
		image = cvQueryFrame(capture);
		if (image == NULL) {
			myDocument->SaveFile(savepath);
			delete myDocument;
			break;
		}
		cvCvtColor(image, imagegray, CV_BGR2GRAY);
		cvConvert(imagegray,imagegrayMat);
		//int thresh = Otsu(imagegray);
		computeback();
		computefore();		
		FillInternalContours(fore, tresh);
		computeObjects();
		//
		object* formemory = currobjects->next;
		TiXmlElement *frame = new TiXmlElement("Frame");
		frame->SetAttribute("numFrame", nFrmNum);
		RootElement->LinkEndChild(frame);
		while (formemory){		
			//frame add object
			TiXmlElement *object = new TiXmlElement("object");
			object->SetAttribute("index", formemory->index);
			frame->LinkEndChild(object);
			//object add x & y
			TiXmlElement *x = new TiXmlElement("X");
			object->LinkEndChild(x);
			sprintf_s(buf,10,"%d",formemory->x);
			TiXmlText *xtext = new TiXmlText(buf);
			x->LinkEndChild(xtext);
			TiXmlElement *y = new TiXmlElement("Y");
			object->LinkEndChild(y);
			sprintf_s(buf, 10, "%d", formemory->y);
			TiXmlText *ytext = new TiXmlText(buf);
			y->LinkEndChild(ytext);
			//
			formemory = formemory->next;
		}
		formemory= NULL;
		drawObjects();
		cvShowImage("src", image);
		cvShowImage("back", back);
		cvShowImage("fore", fore);
		cvWaitKey(20);
	}
	return 0;
}
