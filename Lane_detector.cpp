
#include <iostream>
#include <fstream>
#include <ctime>
#include <queue>
#include <cv.h>
#include <unistd.h>
#include <highgui.h>
#include "opencv2/opencv.hpp"
#include "opencv2/objdetect.hpp"
#include "opencv2/highgui.hpp"
#include "opencv2/imgproc.hpp"
#include <string.h>
#include <sys/time.h>

using namespace cv;
using namespace std;

const CvScalar COLOR_BLUE = CvScalar(255, 0, 0);
const CvScalar COLOR_RED = CvScalar(0, 0, 255);
const CvScalar COLOR_GREEN = CvScalar(170, 170, 0);

const Vec3b RGB_WHITE_LOWER = Vec3b(100, 100, 160);
const Vec3b RGB_WHITE_UPPER = Vec3b(255, 255, 255);
const Vec3b RGB_YELLOW_LOWER = Vec3b(225, 180, 0);
const Vec3b RGB_YELLOW_UPPER = Vec3b(255, 255, 170);
const Vec3b HSV_YELLOW_LOWER = Vec3b(10, 50, 130);
const Vec3b HSV_YELLOW_UPPER = Vec3b(40, 140, 255);

const Vec3b HLS_YELLOW_LOWER = Vec3b(20, 120, 80);
const Vec3b HLS_YELLOW_UPPER = Vec3b(45, 200, 255);

const Size imageSize = Size(640, 480);


Mat cameraMatrix = Mat::eye(3, 3, CV_64FC1);
Mat distCoeffs = Mat::zeros(1, 5, CV_64FC1);
Mat mask = getStructuringElement(MORPH_RECT, Size(3, 3), Point(1, 1));

void base_ROI(Mat& img, Mat& img_ROI);
void v_roi(Mat& img, Mat& img_ROI, const Point& p1, const Point& p2);
float get_slope(const Point& p1, const Point& p2);
bool hough_left(Mat& img, Point* p1, Point* p2);
bool hough_right(Mat& img, Point* p1, Point* p2);
bool hough_curve(Mat& img, Point* p1, Point* p2);
float data_transform(float x, float in_min, float in_max, float out_min, float out_max);
bool get_intersectpoint(const Point& AP1, const Point& AP2,
const Point& BP1, const Point& BP2, Point* IP);

int main(){

  //input camera
  VideoCapture capture;
  capture.open(-1);

  if (!capture.isOpened()){
    cerr << "Camera error" << endl;
    return -1;
  }


  //lane detection
  Mat frame, binary, binaryL, binaryR, leftROI, rightROI, grayImgL, grayImgR, cannyImgL, cannyImgR;
  Point p1, p2, p3, p4, p5;
  Mat dilated, dilatedL, dilatedR;


  //curve detection
  Mat oriImg, grayImg;
  Point cp1, cp2;

  //Camera Calibration
  Mat temp, map1, map2;

  //Control var
  float steer, skewness, xLeft, xRight, y = 120, slope, x_Difference;


  //output
  Mat output;


  FileStorage fs;
  fs.open("camcalib.xml", FileStorage::READ);

  if(!fs.isOpened()){
    cerr << "Failed to opend"<< endl;
    return -1;
  }

  fs["cameraMatrix"] >> cameraMatrix;
  fs["distCoeffs"] >> distCoeffs;

  fs.release();

  initUndistortRectifyMap(cameraMatrix, distCoeffs, Mat(), cameraMatrix, imageSize, CV_32FC1, map1, map2);

  bool left_error = true, right_error = true, curve_error = true;

  while(1){

    capture >> frame;
    temp = frame.clone();
    remap(frame, temp, map1, map2, CV_INTER_LINEAR);

    leftROI = temp(Rect(temp.cols/16, temp.rows/4 * 3, temp.cols/16 * 7, temp.rows/4));
    rightROI = temp(Rect(temp.cols/16 * 8, temp.rows/4 * 3, temp.cols/16 * 7, temp.rows/4));

    inRange(leftROI, RGB_WHITE_LOWER, RGB_WHITE_UPPER, binaryL);
    inRange(rightROI, RGB_WHITE_LOWER, RGB_WHITE_UPPER, binaryR);

    dilate(binaryL, dilatedL, mask, Point(-1, -1), 3);
    dilate(binaryR, dilatedR, mask, Point(-1, -1), 3);

    Canny(dilatedL, cannyImgL, 150, 270);
    Canny(dilatedR, cannyImgR, 150, 270);

    left_error = hough_left(cannyImgL, &p1, &p2);
    right_error = hough_right(cannyImgR, &p3, &p4);

    if(left_error || right_error){

      oriImg = temp(Rect(temp.cols/16, temp.rows/4 * 3, temp.cols/16*14, temp.rows/4));

      inRange(oriImg, RGB_WHITE_LOWER, RGB_WHITE_UPPER, binary);
      dilate(binary, dilated, mask, Point(-1, -1), 3);

      Canny(dilated, cannyImgL, 150, 270);

      curve_error = hough_curve(cannyImgL, &cp1, &cp2);

      slope = get_slope(cp1, cp2);

      if(curve_error){
        steer = 0.0;
      }
      else if(slope < 0.0){
      //right rotate (-1.0 ~ 0.0)
      xLeft = (y - cp1.y + slope * cp1.x) / slope;

      if(xLeft > 320.0){
        steer = -1.0;
      }
      else{

        steer =  data_transform(slope, -1.2, -0.1, 0.0, 2.0);

        skewness = data_transform(xLeft, -200.0, 320.0, 0.0, 2.0);
        steer = -(steer * skewness);

      }

      }
      else{
      //left rotate (0.0 ~ 1.0)
      xRight = (y - cp1.y + slope * cp1.x) / slope;

      if(xRight < 320.0){
        steer = 1.0;
      }
      else{

        steer =  data_transform(slope, 0.1, 1.2, -2.0, 0.0);
        skewness = data_transform(xRight, 320.0, 800.0, -2.0, 0.0);
        steer = (steer * skewness);
      }

      }


      line(oriImg, cp1, cp2, COLOR_RED, 4, CV_AA);

      imshow("curve_output", oriImg);

    }
    else{
      line(leftROI, p1, p2, COLOR_RED, 4, CV_AA);
      line(rightROI, p3, p4, COLOR_RED, 4, CV_AA);

      hconcat(leftROI, rightROI, output);

      get_intersectpoint(p1, p2, Point(p3.x + 320, p3.y), Point(p4.x + 320, p4.y), &p5);

      x_Difference = 220.0 - p5.x;

      if(x_Difference > 0.0){
        steer = data_transform(x_Difference, 0.0, 200.0, 0.0, 0.2);
        steer = -steer;
      }
      else if(x_Difference < 0.0){
        steer = data_transform(x_Difference, -440.0, 0.0, -0.2, 0.0);
        steer = -steer;
      }
      else{
        steer = 0.0;
      }


      imshow("reuslt", output);

    }

    steer =  2.0 * steer * steer * steer;


    if(steer > 1.0){
      steer = 1.0;
    }
    else if(steer < -1.0){
      steer = -1.0;
    }

    printf("%lf\n", steer);

    //pipe file
    FILE* fp = fopen("/home/nvidia/Desktop/p2", "w");
    fprintf(fp, "%f", steer);
    fclose(fp);

    if (waitKey(10) == 0) {
      return 0;
    }

  }
}

void base_ROI(Mat& img, Mat& img_ROI) {

	Point a = Point(0, 40);
	Point b = Point(0, img.rows);
	Point c = Point(img.cols, img.rows);
	Point d = Point(img.cols, 40);

	vector <Point> Left_Point;

	Left_Point.push_back(a);
	Left_Point.push_back(b);
	Left_Point.push_back(c);
	Left_Point.push_back(d);

	Mat roi(img.rows, img.cols, CV_8U, Scalar(0));

	fillConvexPoly(roi, Left_Point, Scalar(255));

	Mat filteredImg_Left;
	img.copyTo(filteredImg_Left, roi);

	img_ROI = filteredImg_Left.clone();

}

void v_roi(Mat& img, Mat& img_ROI, const Point& p1, const Point& p2){

  float slope = get_slope(p1, p2);
  float alphaY = 40.f / sqrt(slope*slope + 1);
  float alphaX = slope * alphaY;

  Point a(p1.x - alphaX, p1.y + alphaY );
  Point b(p1.x + alphaX, p1.y - alphaY );
  Point c(p2.x + alphaX, p2.y - alphaY );
  Point d(p2.x - alphaX, p2.y + alphaY );

  vector <Point> Left_Point;

  Left_Point.push_back(a);
  Left_Point.push_back(b);
  Left_Point.push_back(c);
  Left_Point.push_back(d);

  Mat roi(img.rows, img.cols, CV_8U, Scalar(0));

  fillConvexPoly(roi, Left_Point, Scalar(255));

  Mat filteredImg_Left;
  img.copyTo(filteredImg_Left, roi);


  img_ROI = filteredImg_Left.clone();

}


float get_slope(const Point& p1, const Point& p2){

  float slope;

  slope = ((float) p2.y - (float) p1.y) / ((float) p2.x - (float) p1.x);

  return slope;
}

bool get_intersectpoint(const Point& AP1, const Point& AP2,
                       const Point& BP1, const Point& BP2, Point* IP)
{
    double t;
    double s;
    double under = (BP2.y-BP1.y)*(AP2.x-AP1.x)-(BP2.x-BP1.x)*(AP2.y-AP1.y);
    if(under==0) return false;

    double _t = (BP2.x-BP1.x)*(AP1.y-BP1.y) - (BP2.y-BP1.y)*(AP1.x-BP1.x);
    double _s = (AP2.x-AP1.x)*(AP1.y-BP1.y) - (AP2.y-AP1.y)*(AP1.x-BP1.x);

    t = _t/under;
    s = _s/under;

    if(t<0.0 || t>1.0 || s<0.0 || s>1.0) return false;
    if(_t==0 && _s==0) return false;

    IP->x = AP1.x + t * (double)(AP2.x-AP1.x);
    IP->y = AP1.y + t * (double)(AP2.y-AP1.y);

    return true;
}

bool hough_left(Mat& img, Point* p1, Point* p2) {

	vector<Vec2f> linesL;
  vector<Vec2f> newLinesL;

  Point point1;
  Point point2;

  int count = 0, x1 = 0, x2 = 0, y1 = 0, y2 = 0;
  int threshold = 40;

  for (int i = 10; i > 0; i--){

    HoughLines(img, linesL, 1, CV_PI / 180, threshold);

    for(size_t j = 0; j < linesL.size(); j++){

      Vec2f temp;

      float rho = linesL[j][0];
      float theta = linesL[j][1];

      if(CV_PI / 18 >= theta || theta >= CV_PI / 18 * 8) continue;

      temp[0] = rho;
      temp[1] = theta;

      newLinesL.push_back(temp);

    }


    int clusterCount = 2;
  		Mat h_points = Mat(newLinesL.size(), 1, CV_32FC2);
  		Mat labels, centers;
  		if (newLinesL.size() > 1) {
  			for (size_t i = 0; i < newLinesL.size(); i++) {
  				count++;
  				float rho = newLinesL[i][0];
  				float theta = newLinesL[i][1];


  				double a = cos(theta), b = sin(theta);
  				double x0 = a * rho, y0 = b * rho;
  				h_points.at<Point2f>(i, 0) = Point2f(rho, (float)(theta * 100));
  			}
  			kmeans(h_points, clusterCount, labels,
  				TermCriteria(TermCriteria::COUNT + TermCriteria::EPS, 10, 1.0),
  				3, KMEANS_RANDOM_CENTERS, centers);

  			Point mypt1 = centers.at<Point2f>(0, 0);

  			float rho = mypt1.x;
  			float theta = (float)mypt1.y / 100;
  			double a = cos(theta), b = sin(theta);
  			double x0 = a * rho, y0 = b * rho;

  			int _x1 = int(x0 + 1000 * (-b));
  			int _y1 = int(y0 + 1000 * (a));
  			int _x2 = int(x0 - 1000 * (-b));
  			int _y2 = int(y0 - 1000 * (a));

  			x1 += _x1;
  			y1 += _y1;

  			x2 += _x2;
  			y2 += _y2;

  			Point mypt2 = centers.at<Point2f>(1, 0);

  			rho = mypt2.x;
  			theta = (float)mypt2.y / 100;
  			a = cos(theta), b = sin(theta);
  			x0 = a * rho, y0 = b * rho;

  			_x1 = int(x0 + 1000 * (-b));
  			_y1 = int(y0 + 1000 * (a));
  			_x2 = int(x0 - 1000 * (-b));
  			_y2 = int(y0 - 1000 * (a));

  			x1 += _x1;
  			y1 += _y1;

  			x2 += _x2;
  			y2 += _y2;

  			break;
  		};
  	}
  	if (count != 0) {
  		p1->x = x1 / 2; p1->y = y1 / 2;
  		p2->x = x2 / 2; p2->y = y2 / 2;


  		return false;
  	}
  	return true;
}

bool hough_right(Mat& img, Point* p1, Point* p2) {

	vector<Vec2f> linesR;
  vector<Vec2f> newLinesR;

  Point point1;
  Point point2;

  int count = 0, x1 = 0, x2 = 0, y1 = 0, y2 = 0;
  int threshold = 40;

  for (int i = 10; i > 0; i--){
    HoughLines(img, linesR, 1, CV_PI / 180, threshold);



    for(size_t j = 0; j < linesR.size(); j++){

      Vec2f temp;

      float rho = linesR[j][0];
      float theta = linesR[j][1];

      if(CV_PI / 18 * 10 >= theta || theta >= CV_PI / 18 * 17) continue;

      temp[0] = rho;
      temp[1] = theta;

      newLinesR.push_back(temp);

    }


    int clusterCount = 2;
  		Mat h_points = Mat(newLinesR.size(), 1, CV_32FC2);
  		Mat labels, centers;
  		if (newLinesR.size() > 1) {
  			for (size_t i = 0; i < newLinesR.size(); i++) {
  				count++;
  				float rho = newLinesR[i][0];
  				float theta = newLinesR[i][1];


  				double a = cos(theta), b = sin(theta);
  				double x0 = a * rho, y0 = b * rho;
  				h_points.at<Point2f>(i, 0) = Point2f(rho, (float)(theta * 100));
  			}
  			kmeans(h_points, clusterCount, labels,
  				TermCriteria(TermCriteria::COUNT + TermCriteria::EPS, 10, 1.0),
  				3, KMEANS_RANDOM_CENTERS, centers);

  			Point mypt1 = centers.at<Point2f>(0, 0);

  			float rho = mypt1.x;
  			float theta = (float)mypt1.y / 100;
  			double a = cos(theta), b = sin(theta);
  			double x0 = a * rho, y0 = b * rho;

  			int _x1 = int(x0 + 1000 * (-b));
  			int _y1 = int(y0 + 1000 * (a));
  			int _x2 = int(x0 - 1000 * (-b));
  			int _y2 = int(y0 - 1000 * (a));

  			x1 += _x1;
  			y1 += _y1;

  			x2 += _x2;
  			y2 += _y2;

  			Point mypt2 = centers.at<Point2f>(1, 0);

  			rho = mypt2.x;
  			theta = (float)mypt2.y / 100;
  			a = cos(theta), b = sin(theta);
  			x0 = a * rho, y0 = b * rho;

  			_x1 = int(x0 + 1000 * (-b));
  			_y1 = int(y0 + 1000 * (a));
  			_x2 = int(x0 - 1000 * (-b));
  			_y2 = int(y0 - 1000 * (a));

  			x1 += _x1;
  			y1 += _y1;

  			x2 += _x2;
  			y2 += _y2;

  			break;
  		};
  	}
  	if (count != 0) {
  		p1->x = x1 / 2; p1->y = y1 / 2;
  		p2->x = x2 / 2; p2->y = y2 / 2;

  		return false;
  	}
  	return true;
}


bool hough_curve(Mat& img, Point* p1, Point* p2) {

	vector<Vec2f> lines;

  Point point1;
  Point point2;

  int count = 0, x1 = 0, x2 = 0, y1 = 0, y2 = 0;
  int threshold = 40;

  for (int i = 10; i > 0; i--){
    HoughLines(img, lines, 1, CV_PI / 180, threshold);


    int clusterCount = 2;
  		Mat h_points = Mat(lines.size(), 1, CV_32FC2);
  		Mat labels, centers;
  		if (lines.size() > 1) {
  			for (size_t i = 0; i < lines.size(); i++) {
  				count++;
  				float rho = lines[i][0];
  				float theta = lines[i][1];


  				double a = cos(theta), b = sin(theta);
  				double x0 = a * rho, y0 = b * rho;
  				h_points.at<Point2f>(i, 0) = Point2f(rho, (float)(theta * 100));
  			}
  			kmeans(h_points, clusterCount, labels,
  				TermCriteria(TermCriteria::COUNT + TermCriteria::EPS, 10, 1.0),
  				3, KMEANS_RANDOM_CENTERS, centers);

  			Point mypt1 = centers.at<Point2f>(0, 0);

  			float rho = mypt1.x;
  			float theta = (float)mypt1.y / 100;
  			double a = cos(theta), b = sin(theta);
  			double x0 = a * rho, y0 = b * rho;

  			int _x1 = int(x0 + 1000 * (-b));
  			int _y1 = int(y0 + 1000 * (a));
  			int _x2 = int(x0 - 1000 * (-b));
  			int _y2 = int(y0 - 1000 * (a));

  			x1 += _x1;
  			y1 += _y1;

  			x2 += _x2;
  			y2 += _y2;

  			Point mypt2 = centers.at<Point2f>(1, 0);

  			rho = mypt2.x;
  			theta = (float)mypt2.y / 100;
  			a = cos(theta), b = sin(theta);
  			x0 = a * rho, y0 = b * rho;

  			_x1 = int(x0 + 1000 * (-b));
  			_y1 = int(y0 + 1000 * (a));
  			_x2 = int(x0 - 1000 * (-b));
  			_y2 = int(y0 - 1000 * (a));

  			x1 += _x1;
  			y1 += _y1;

  			x2 += _x2;
  			y2 += _y2;

  			break;
  		};
  	}
  	if (count != 0) {
  		p1->x = x1 / 2; p1->y = y1 / 2;
  		p2->x = x2 / 2; p2->y = y2 / 2;

  		return false;
  	}
  	return true;
}

float data_transform(float x, float in_min, float in_max, float out_min, float out_max){
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}
