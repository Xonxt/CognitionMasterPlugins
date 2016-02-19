#include "stdafx.h"

#include <stdlib.h>
#include <stdint.h>
#include <limits.h>
#include <assert.h>
#include <math.h>
#include <iostream>

#include "openslide\openslide.h"

#include "openjpeg.h"
#include "opj_stdint.h"

#include "opencv2\core\core.hpp"
#include "opencv2\highgui\highgui.hpp"
#include "opencv2\imgproc\imgproc.hpp"

int main(int argc, char* argv[])
{

	char input_file[128];
	char output_file[64];

	int32_t level = 4;

	if (argc == 4) {
		strcpy(input_file, argv[1]);
		strcpy(output_file, argv[2]);
//		level = atoi(argv[2]);
	}
	else {
		strcpy(input_file, "D:\\temp\\Charite\\Camelyon16\\Tumor\\Tumor_001.tif");
		strcpy(output_file, "Tumor_001.jp2");
//		level = 5;
	}

	openslide_t* slide = openslide_open(input_file);
	//openslide_t* mask = openslide_open("C:\\Users\\kovalenm\\Work\\imgs\\Mask\\Tumor_001_Mask.tif");

	int32_t levCount = openslide_get_level_count(slide);

	//level = (openslide_get_level_count(mask) < openslide_get_level_count(slide)) ? openslide_get_level_count(mask) - 1 : openslide_get_level_count(slide) - 1;
	//level = (openslide_get_level_count(mask) < openslide_get_level_count(slide)) ? openslide_get_level_count(mask) - 1 : openslide_get_level_count(slide) - 1;

	//level = ;	

	int64_t x = 0, y = 0, width, height;

	openslide_get_level_dimensions(slide, level, &width, &height);

	double *ds = new double[levCount];
	for (int i = 0; i < levCount; i++) {
		ds[i] = openslide_get_level_downsample(slide, i);
	}

	// get tile width/height at current level
	int64_t size = 4096 * ds[0] / ds[level];

	//size = 512;
//	width = (int64_t)ceil((double)width / (double)size) * (int64_t)size;
//	height = (int64_t)ceil((double)height / (double)size) * (int64_t)size;

	size_t tileSize = size * size * 4;
	uint32_t *tile = (uint32_t*)malloc(tileSize);

	//size_t imgSize = width * height * 3;
	//uint32_t *imgData = (uint32_t*)malloc(imgSize);
	cv::Mat fullImg(height, width, CV_8UC3);

	OPJ_UINT32 l_nb_tiles_x = (OPJ_UINT32)(width / size);
	OPJ_UINT32 l_nb_tiles_y = (OPJ_UINT32)(height / size);

	int64_t step = 4096;
	OPJ_INT32 offset = width * height, smallStep = size*size;

	int64_t numTile = 0;
	std::cout << "Copying data to new openCV image... " << std::endl;

	cv::Mat roi;
	for (int y = 0; y < l_nb_tiles_y; ++y) {
		for (int x = 0; x < l_nb_tiles_x; ++x) {

			std::cout << "Doing tile " << numTile << " - [" << x << "; " << y << "]" << std::endl;

			openslide_read_region(slide, tile, x * step, y * step, level, size, size);

			cv::Mat img(size, size, CV_8UC4, tile);
			cv::Mat img2;
			img.copyTo(img2);

			img.release();

			cv::Mat img3;

			cv::cvtColor(img2, img3, CV_RGBA2BGR);
			img2.release();

			// copy to full img
			roi = cv::Mat(fullImg, cv::Rect(x*size, y*size, size, size));
			img3.copyTo(roi);
			img3.release();

			//std::memcpy(fullImg.data + offset * 3, bgra[3].data, bytesize * sizeof(OPJ_BYTE));

			numTile++;
		}
	}

	//cv::Mat img3;
	//cv::cvtColor(fullImg, img3, CV_BGR2RGB);

	std::cout << "Saving image to file... ";
	cv::imwrite(output_file, fullImg);
	
	std::cout << "DONE!" << std::endl;

	/*std::cout << "Reading image from TIFF... ";
	openslide_read_region(slide, imgData, 0, 0, level, width, height);
	std::cout << "DONE!" << std::endl;

	std::cout << "Creating an empty openCV image... ";
	cv::Mat img(height, width, CV_8UC4, imgData);
	std::cout << "DONE!" << std::endl;

	std::cout << "Copying data to new openCV image... ";
	cv::Mat img2;
	img.copyTo(img2);
	std::cout << "DONE!" << std::endl;

	cv::Mat img3;
	std::cout << "Converting to RGBA to BGR... ";
	cv::cvtColor(img2, img3, CV_BGRA2BGR);
	std::cout << "DONE!" << std::endl;

	std::cout << "Saving image to file... ";
	cv::imwrite(output_file, img3);
	std::cout << "DONE!" << std::endl;*/

	openslide_close(slide);
	//openslide_close(mask);

	return 0;
}