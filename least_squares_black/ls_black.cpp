#include "opencv2/opencv.hpp"
#include <iostream>
#include <cmath>
#include <vector>

using namespace std;
using namespace cv;

RotatedRect fitEllipse_edit( InputArray _points , double & error); //edited opencv libraries
CvBox2D cvFitEllipse_edit( const CvArr* array,double & error );

int main(int argc, char * argv[])
{	
	//tunable parameters in order of appearance in code below
	int black_level = 20;//lower value means less pixels are considered black
	int gray_closeness = 20;//lower value means pixels must be nearer in color to be considered true gray
	int gray_level = 40;//lower value means less pixels are considered dark grey
	int number_of_constituent_points = 10;//means curves must be long enough
	int least_squares_error = 2;//higher value returns more circles
	int acceptable_eccentricity = 5;//higher value returns more ellipses not like circles
	int debouncing_age = 20;//longer value means debouncing picks up more circles
	int center_distance = 10;//larger value means circles can move around but still be considered stable
	int histogram_tile = 4;
	int clip_limit;

	VideoCapture capture = VideoCapture(atoi(argv[1]));

	//discard initial blank images
	Mat image;
	capture >> image;
	while(image.empty())
		capture>>image;


	//declare and initialize all variables up front to save time in while loop
	//(these are declared and initialized in order of use in the while loop because i'm ocd)
	Mat bgr_image;
	Mat ycrcb_image;
	vector<Mat> channels;
	Ptr<CLAHE> clahe = createCLAHE();
	Mat equalized_ycrcb_image;
	Mat equalized_bgr_image;
	Mat equalized_gray_image;
	Mat blurred_image;
	Mat edges_image;
	vector<vector<Point> > contour_vector;
	Mat hierarchy_placeholder;
	vector<Mat> bgr_channels;
	int i, j;
	Scalar color = Scalar(0,200,0);
	int image_rows = image.rows, image_cols = image.cols;
	Mat black_or_not_image(image.rows, image.cols, CV_8UC1);
	bool black_flag;
	uchar blue_pixel, green_pixel, red_pixel;
	Mat debug_image;
	int number_of_contours;
	double error;
	RotatedRect rec;
 	Size2f rec_size;
	int rec_size_width, rec_size_height;
	double ellipse_axis1;
	double ellipse_axis2;
	Point2f center;
	int center_x;
	int center_y;
	int radius;
	int radius_squared;
	double number_of_black_pixels;
	double total_number_of_pixels;
	int j_max, k_max;
	int k;
	int x_displacement;
	int y_displacement;
	vector<Point2f> new_centers;
	vector<Point2f> old_centers;
	vector<int> old_centers_ages;
	Point2f this_old_center;
	
	imshow( "Mars", image);

	Mat mat_placeholder(1, 500, CV_8UC1);
	imshow("Tuning",mat_placeholder);
	createTrackbar("number of constituent points", "Tuning", &number_of_constituent_points, 50, NULL, 0);
	createTrackbar("acceptable eccentricity", "Tuning", &acceptable_eccentricity, 50, NULL, 0);
	createTrackbar("least squares error", "Tuning", &least_squares_error, 10, NULL, 0);
	createTrackbar("black level", "Tuning", &black_level, 255, NULL, 0);
	createTrackbar("gray closeness", "Tuning", &gray_closeness, 255, NULL, 0);
	createTrackbar("gray_level", "Tuning", &gray_level, 255, NULL, 0);
	createTrackbar("debouncing age", "Tuning", &debouncing_age, 50, NULL, 0);
	createTrackbar("center distance", "Tuning", &center_distance, 50, NULL, 0);
	//createTrackbar("histogram tile", "Mars", &histogram_tile, 400, NULL, 0);
	//createTrackbar("clip limit?", "Mars", &clip_limit, 400, NULL, 0);

	waitKey(1);//need some lag
	
	while(true)//escape key is 27 on Alex's linux box
	{
		capture>>bgr_image;//clear 5 deep buffer
		capture>>bgr_image;
		capture>>bgr_image;
		capture>>bgr_image;
		capture>>bgr_image;
		
		if(!bgr_image.empty())
		{
			cvtColor(bgr_image, ycrcb_image, CV_BGR2YCrCb);
		        split(ycrcb_image,channels);

			//adaptive histogram (local contrast enhancement)
			/*clahe->setTilesGridSize(Size(histogram_tile,histogram_tile));	
			clahe->apply(channels[0],channels[0]);
			clahe->setClipLimit(clip_limit);*/

			//example of non-adaptive histogram (should stay commented)
			equalizeHist(channels[0], channels[0]);

		        merge(channels,equalized_ycrcb_image);

		        cvtColor(equalized_ycrcb_image,equalized_bgr_image,CV_YCrCb2BGR);
			cvtColor(equalized_bgr_image,equalized_gray_image,CV_BGR2GRAY);
			imshow("histo",			equalized_bgr_image);
			
			GaussianBlur(equalized_gray_image, blurred_image, Size(25,25),3, 3);
			Canny(blurred_image, edges_image, 10, 100, 3 );
			findContours(edges_image, contour_vector, hierarchy_placeholder, CV_RETR_EXTERNAL, CV_CHAIN_APPROX_TC89_L1, Point(0, 0) );

			//find black parts of image starts here
			
			split(equalized_bgr_image,bgr_channels);

			for(i=0; i < image_rows;i++)
			for(j=0; j < image_cols;j++)
			{
									
				black_flag = false;
				blue_pixel = bgr_channels[0].at<uchar>(i,j);
				green_pixel = bgr_channels[1].at<uchar>(i,j);
				red_pixel = bgr_channels[2].at<uchar>(i,j);
	
				//if all colors are too low (black)
				if(blue_pixel < black_level && green_pixel < black_level && red_pixel < black_level)
					black_flag = true;						
				//else if all colors are similar and below a higher threshold (dark grey)
  				else if(abs(blue_pixel - green_pixel) < gray_closeness && abs(green_pixel - red_pixel) < gray_closeness && abs(red_pixel - blue_pixel) < gray_closeness && blue_pixel < gray_level && green_pixel < gray_level && red_pixel < gray_level)
					black_flag = true;
				//thresholding condition applied
				if(black_flag)
					black_or_not_image.at<uchar>(i,j) = 0;
				else							
					black_or_not_image.at<uchar>(i,j) = 255;
			}
			
			//find black parts of image ends here

			
			//bgr_image is with debouncing
			//debug image holds only static circles (not debounced)
			debug_image = bgr_image.clone();
			number_of_contours = contour_vector.size();
			for(i = 0; i < number_of_contours; i++)
			{
				if(contour_vector[i].size() >=number_of_constituent_points)
				{
					//find least squares ellipse and associated error
					rec = fitEllipse_edit(contour_vector[i], error);
					rec_size = rec.size;
					rec_size_width = rec_size.width;
					rec_size_height = rec_size.height;

					//if ellispe is a good least square fit

					if(error < least_squares_error)
					{
						//if ellipse eccentricity is low
						ellipse_axis1 = rec_size_width;
						ellipse_axis2 = rec_size_height;
						if(10 * abs(ellipse_axis1 - ellipse_axis2)/ ellipse_axis1 < acceptable_eccentricity )
						{
							//look for mostly black inside "circle"
							center = CvBox2D(rec).center;
							center_x = center.x;
							center_y = center.y;
							radius = ellipse_axis1 / 2;
							radius_squared = radius * radius;
							number_of_black_pixels = 0;
							total_number_of_pixels = radius * radius * 3.14159;

							j_max = center_x + rec_size_width/2;
							k_max = center_y + rec_size_height/2;
							for(j = center_x - rec_size_width/2; j < j_max; j++)
							for(k = center_y - rec_size_height/2; k < k_max; k++)
							{
								x_displacement = j - center_x;
								y_displacement = k - center_y;
								
								//if it's in the circle
								if(x_displacement * x_displacement + y_displacement * y_displacement < radius_squared)
								{
									//if it's out of range add .5 to the count
									if(j < 0 || j >=  image_rows  || k < 0 || k >= image_cols)
										number_of_black_pixels += .5;
									//if it's black add to the count
									else if(black_or_not_image.at<uchar>(j, k) == 0)
										number_of_black_pixels++;
								}
							}
							//if it's a mostly black circle

							if(number_of_black_pixels / total_number_of_pixels > .2)
							{
								ellipse(debug_image, rec, color, 3,8);//non-debounced (for debugging)

								//debouncing
								new_centers.push_back(center);
								for(j = 0; j < old_centers.size(); j++)
								{
									this_old_center= old_centers.at(j);
									if(abs(this_old_center.x - center_x) < center_distance && abs(this_old_center.y - center_y) < center_distance)
									{
										//remove center from old list if it's found again
										old_centers.erase(old_centers.begin() + j);
										old_centers_ages.erase(old_centers_ages.begin() + j);
										//and display the circle and coordinates
										ellipse(bgr_image, rec, color, 3, 8);
										
										cout << 300 / radius  << "ft";//distance metric
										if(center_x < image_rows / 2 )
											cout << ", left";
										else
											cout << ", right";
										if(center_y < image_cols / 2)
											cout << ", top" << endl;
										else
											cout << ", bottom" << endl;
									}
								}
							}
						}
					}
				}

			}
			//combine old and new centers
			old_centers.reserve(old_centers.size() + new_centers.size()); // preallocate memory for next instruction
			old_centers.insert(old_centers.end(), new_centers.begin(), new_centers.end());
			for(i = 0; i < old_centers_ages.size(); i++)
			{
				if(old_centers_ages.at(i) == 0)
				{
					old_centers.erase(old_centers.begin() + i);
					old_centers_ages.erase(old_centers_ages.begin() + i);
				}
				else
					old_centers_ages.at(i) = old_centers_ages.at(i) - 1;
			}
			for(i = old_centers_ages.size(); i < old_centers.size(); i++)
			{
				old_centers_ages.push_back(2);
			}	
			new_centers.clear();
			imshow( "Mars", bgr_image);	
		}
		waitKey(1);//need some lag
	}
	return 0;
}

RotatedRect fitEllipse_edit( InputArray _points , double & error)
{
    Mat points = _points.getMat();
    CV_Assert(points.checkVector(2) >= 0 &&
              (points.depth() == CV_32F || points.depth() == CV_32S));
    CvMat _cpoints = points;
    return cvFitEllipse_edit(&_cpoints, error);
}


CvBox2D cvFitEllipse_edit( const CvArr* array , double & error)
{
    CvBox2D box;
    cv::AutoBuffer<double> Ad, bd;
    memset( &box, 0, sizeof(box));

    CvContour contour_header;
    CvSeq* ptseq = 0;
    CvSeqBlock block;
    int n;

    if( CV_IS_SEQ( array ))
    {
        ptseq = (CvSeq*)array;
        if( !CV_IS_SEQ_POINT_SET( ptseq ))
            CV_Error( CV_StsBadArg, "Unsupported sequence type" );
    }
    else
    {
        ptseq = cvPointSeqFromMat(CV_SEQ_KIND_GENERIC, array, &contour_header, &block);
    }

    n = ptseq->total;
    if( n < 5 )
        CV_Error( CV_StsBadSize, "Number of points should be >= 5" );

    /*
     *  New fitellipse algorithm, contributed by Dr. Daniel Weiss
     */
    CvPoint2D32f c = {0,0};
    double gfp[5], rp[5], t;
    CvMat A, b, x;
    const double min_eps = 1e-8;
    int i, is_float;
    CvSeqReader reader;

    Ad.allocate(n*5);
    bd.allocate(n);

    // first fit for parameters A - E
    A = cvMat( n, 5, CV_64F, Ad );
    b = cvMat( n, 1, CV_64F, bd );
    x = cvMat( 5, 1, CV_64F, gfp );

    cvStartReadSeq( ptseq, &reader );
    is_float = CV_SEQ_ELTYPE(ptseq) == CV_32FC2;

    for( i = 0; i < n; i++ )
    {
        CvPoint2D32f p;
        if( is_float )
            p = *(CvPoint2D32f*)(reader.ptr);
        else
        {
            p.x = (float)((int*)reader.ptr)[0];
            p.y = (float)((int*)reader.ptr)[1];
        }
        CV_NEXT_SEQ_ELEM( sizeof(p), reader );
        c.x += p.x;
        c.y += p.y;
    }
    c.x /= n;
    c.y /= n;

    for( i = 0; i < n; i++ )
    {
        CvPoint2D32f p;
        if( is_float )
            p = *(CvPoint2D32f*)(reader.ptr);
        else
        {
            p.x = (float)((int*)reader.ptr)[0];
            p.y = (float)((int*)reader.ptr)[1];
        }
        CV_NEXT_SEQ_ELEM( sizeof(p), reader );
        p.x -= c.x;
        p.y -= c.y;

        bd[i] = 10000.0; // 1.0?
        Ad[i*5] = -(double)p.x * p.x; // A - C signs inverted as proposed by APP
        Ad[i*5 + 1] = -(double)p.y * p.y;
        Ad[i*5 + 2] = -(double)p.x * p.y;
        Ad[i*5 + 3] = p.x;
        Ad[i*5 + 4] = p.y;
    }

    cvSolve( &A, &b, &x, CV_SVD );

    // now use general-form parameters A - E to find the ellipse center:
    // differentiate general form wrt x/y to get two equations for cx and cy
    A = cvMat( 2, 2, CV_64F, Ad );
    b = cvMat( 2, 1, CV_64F, bd );
    x = cvMat( 2, 1, CV_64F, rp );
    Ad[0] = 2 * gfp[0];
    Ad[1] = Ad[2] = gfp[2];
    Ad[3] = 2 * gfp[1];
    bd[0] = gfp[3];
    bd[1] = gfp[4];
    cvSolve( &A, &b, &x, CV_SVD );

    // re-fit for parameters A - C with those center coordinates
    A = cvMat( n, 3, CV_64F, Ad );
    b = cvMat( n, 1, CV_64F, bd );
    x = cvMat( 3, 1, CV_64F, gfp );
    for( i = 0; i < n; i++ )
    {

        CvPoint2D32f p;
        if( is_float )
            p = *(CvPoint2D32f*)(reader.ptr);
        else
        {
            p.x = (float)((int*)reader.ptr)[0];
            p.y = (float)((int*)reader.ptr)[1];
        }
        CV_NEXT_SEQ_ELEM( sizeof(p), reader );
        p.x -= c.x;
        p.y -= c.y;
        bd[i] = 1.0;
        Ad[i * 3] = (p.x - rp[0]) * (p.x - rp[0]);
        Ad[i * 3 + 1] = (p.y - rp[1]) * (p.y - rp[1]);
        Ad[i * 3 + 2] = (p.x - rp[0]) * (p.y - rp[1]);
    }
    cvSolve(&A, &b, &x, CV_SVD);


	const CvMat* A_const = &A;
	const CvMat* b_const = 	&b;
	const CvMat* x_const = &x;
	Mat A_edit(A_const,false);
	Mat x_edit(x_const,false);
	Mat b_edit(b_const,false);
   error = norm(A_edit * x_edit - b_edit);

    // store angle and radii
    rp[4] = -0.5 * atan2(gfp[2], gfp[1] - gfp[0]); // convert from APP angle usage
    t = sin(-2.0 * rp[4]);
    if( fabs(t) > fabs(gfp[2])*min_eps )
        t = gfp[2]/t;
    else
        t = gfp[1] - gfp[0];
    rp[2] = fabs(gfp[0] + gfp[1] - t);
    if( rp[2] > min_eps )
        rp[2] = sqrt(2.0 / rp[2]);
    rp[3] = fabs(gfp[0] + gfp[1] + t);
    if( rp[3] > min_eps )
        rp[3] = sqrt(2.0 / rp[3]);

    box.center.x = (float)rp[0] + c.x;
    box.center.y = (float)rp[1] + c.y;
    box.size.width = (float)(rp[2]*2);
    box.size.height = (float)(rp[3]*2);
    if( box.size.width > box.size.height )
    {
        float tmp;
        CV_SWAP( box.size.width, box.size.height, tmp );
        box.angle = (float)(90 + rp[4]*180/CV_PI);
    }
    if( box.angle < -180 )
        box.angle += 360;
    if( box.angle > 360 )
        box.angle -= 360;

    return box;
}
