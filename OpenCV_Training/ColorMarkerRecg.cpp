#include <opencv2/opencv.hpp>

#include <iostream>

using namespace cv;
using namespace std;

class TrackerOnPic {
public:
	Mat trackWin;
public:
	TrackerOnPic(int* iLowH, int* iLowS, int* iLowV, int* iHighH, int* iHighS, int* iHighV) {
		namedWindow("Control", WINDOW_AUTOSIZE); //create a window called "Control"
		//chosen_minV = Scalar(50, 100, 90);
		//chosen_maxV = Scalar(80, 255, 255);
		//Green as my pen:
		*iLowH = 50;
		*iHighH = 100;

		*iLowS = 100;
		*iHighS = 255;

		*iLowV = 90;
		*iHighV = 255;

		SetupTrkWin(iLowH, iLowS, iLowV, iHighH, iHighS, iHighV);
	}
	void SetupTrkWin(int* iLowH, int* iLowS, int* iLowV, int* iHighH, int* iHighS, int* iHighV) {
		//Create trackbars in "Control" window:
		//The trackerbars changes the variables' values
		createTrackbar("LowH", "Control", iLowH, 179); //Hue (0 - 179)
		createTrackbar("HighH", "Control", iHighH, 179);

		createTrackbar("LowS", "Control", iLowS, 255); //Saturation (0 - 255)
		createTrackbar("HighS", "Control", iHighS, 255);

		createTrackbar("LowV", "Control", iLowV, 255); //Value (0 - 255)
		createTrackbar("HighV", "Control", iHighV, 255);
	}
	Mat GetMaskedFrameHSV(Mat* org, Mat* src, Scalar min_v, Scalar max_v) {
		Mat mask;
		Mat output;
		inRange(*src, min_v, max_v, mask);
		(*org).copyTo(output, mask);
		return output;
	}
};

class MorphOnPic {
public:
	Mat morphWin;
	int morph_elem = 0;		// suggested: 0
	int morph_size = 1;		// suggested: 1
	int morph_operator = 1;	// suggested: 1
	int const max_operator = 4;
	int const max_elem = 2;
	int const max_kernel_size = 21;
public:
	MorphOnPic() {
		/// Create window
		namedWindow("Morph", WINDOW_AUTOSIZE);
		SetupMorphWin();
	}
	void SetupMorphWin() {
		/// Create Trackbar to select Morphology operation
		createTrackbar("Operator:\n 0: Opening - 1: Closing \n 2: Gradient - 3: Top Hat \n 4: Black Hat", "Morph", &morph_operator, max_operator);

		/// Create Trackbar to select kernel type
		createTrackbar("Element:\n 0: Rect - 1: Cross - 2: Ellipse", "Morph",
			&morph_elem, max_elem);

		/// Create Trackbar to choose kernel size
		createTrackbar("Kernel size:\n 2n +1", "Morph",
			&morph_size, max_kernel_size);
	}
	Mat Morphology_Operations(Mat* src) {
		Mat output;
		int op = morph_operator + 2;
		Mat kernel = getStructuringElement(morph_elem, Size(2 * morph_size + 1, 2 * morph_size + 1), Point(morph_size, morph_size));
		morphologyEx(*src, output, op, kernel);
		return output;
	}
};

class MyMarker {
public:
	vector<Mat> _contours;
	vector<cv::Vec4i> _hierarchy;
	cv::Moments max_moment;
	cv::Point2f max_massCenter;
public:
	MyMarker() {
		_contours = vector<Mat>();
		_hierarchy = vector<cv::Vec4i>();
	}
	void SetContours(Mat* src) {
		Mat myCanny;
		Canny(*src, myCanny, 0x3f, 0x3f * 3, 3);	//required for finding contours
		findContours(myCanny, this->_contours, this->_hierarchy, RETR_TREE,
			CHAIN_APPROX_SIMPLE,
			Point(0, 0));
	}

	void DrawLargestContour(Mat* src, Scalar color) {
		int largest_index = 0;
		int max_area = 0;
		for (int i = 0; i < this->_contours.size(); i++) {
			double area_i = contourArea(this->_contours[i]);
			if (area_i > max_area) {
				largest_index = i;
				max_area = area_i;
			}
		}
		if (largest_index < this->_contours.size()) {
			drawContours(*src, this->_contours, largest_index, color, 2, 8, this->_hierarchy, 0, Point());
			CalcMoments(largest_index);
			CalcMassCenter();
			circle(*src, max_massCenter, 4, color);
		}
	}

	void CalcMoments(int i) {
		max_moment = moments(this->_contours[i], false);
	}

	void CalcMassCenter() {
		max_massCenter = cv::Point2f(max_moment.m10 / max_moment.m00, max_moment.m01 / max_moment.m00);
	}

};

void main() {
	VideoCapture cap(0);
	Mat orgImg, hsvImg, imgThresholded;
	Mat frame, sub_frame;

	Mat chosen_color, default_color;
	Scalar chosen_minV, chosen_maxV;
	Scalar minV, maxV; 

	MyMarker green = MyMarker();
	MyMarker blue = MyMarker();
	Mat greenMat, blueMat;

	int green_i=0, blue_i=0;

	int iLowH, iLowS, iLowV;
	int iHighH, iHighS, iHighV;

	if (!cap.isOpened())  // if not success, exit program
	{
		cout << "Cannot open the web cam" << endl;
		return;
	}

	namedWindow("Orig_Frame", 1);
	
	TrackerOnPic trWin = TrackerOnPic(&iLowH, &iLowS, &iLowV, &iHighH, &iHighS, &iHighV);
	MorphOnPic moWin = MorphOnPic();
	
	while (true) {
		bool bSuccess = cap.read(orgImg); // read a new frame from video

		if (!bSuccess) //if not success, break loop
		{
			cout << "Cannot read a frame from video stream" << endl;
			break;
		}

		cvtColor(orgImg, hsvImg, COLOR_BGR2HSV);
		imshow("Orig_Frame", orgImg);
	
		//	Here for calibration of initial values of detection
		//chosen_minV = Scalar(iLowH, iLowS, iLowV);
		//chosen_maxV = Scalar(iHighH, iHighS, iHighV);

		//	Default for Green - HSV format
		chosen_minV = Scalar(50, 100, 90);
		chosen_maxV = Scalar(90, 255, 255);

		//	Default for Blue - HSV format
		minV = Scalar(90, 100, 90);
		maxV = Scalar(130, 255, 255);
		
		//	Generate a mask for a specific color of Marker
		chosen_color = trWin.GetMaskedFrameHSV(&orgImg, &hsvImg, chosen_minV, chosen_maxV);
		default_color = trWin.GetMaskedFrameHSV(&orgImg, &hsvImg, minV, maxV);
		
		//	Show the proccessed frame after applying some closing filters
		greenMat = moWin.Morphology_Operations(&chosen_color);
		blueMat = moWin.Morphology_Operations(&default_color);

		//	Applying all we found on the main frame
		green.SetContours(&greenMat);
		green.DrawLargestContour(&orgImg, Scalar(0, 255, 0));
		green.DrawLargestContour(&greenMat, Scalar(0, 255, 0));
		blue.SetContours(&blueMat);
		blue.DrawLargestContour(&blueMat, Scalar(255, 0, 0));
		blue.DrawLargestContour(&orgImg, Scalar(255, 0, 0));
		printf("g_i: %d, size_g: %d, b_i: %d, size_b = %d\n",green_i, green._contours.size(), blue_i, blue._contours.size());
		line(orgImg, blue.max_massCenter, Point(green.max_massCenter.x, blue.max_massCenter.y), Scalar(255, 0, 0));
		line(orgImg, green.max_massCenter, blue.max_massCenter, Scalar(0, 255, 0));

		int alpha = atan((abs(green.max_massCenter.y - blue.max_massCenter.y))*1.0 /
						(abs(green.max_massCenter.x - blue.max_massCenter.x))*1.0) * (180.0/ 3.14159265);
		putText(orgImg, to_string(alpha) + "Deg", Point2f(0, orgImg.rows-1), FONT_HERSHEY_DUPLEX, 1, Scalar(0, 0, 0));
		imshow("FinalImage", orgImg);
		
		if (waitKey(30) == 'q') break;
	}
}