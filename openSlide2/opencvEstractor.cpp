// OpenSlideExtractor1.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"

#include <stdlib.h>
#include <stdint.h>
#include <limits.h>
#include <assert.h>
#include <math.h>

#include <iostream>
#include <fstream>

#include "openslide\openslide.h"

#include "openjpeg.h"
#include "opj_stdint.h"

#include "opencv2\core\core.hpp"
#include "opencv2\highgui\highgui.hpp"
#include "opencv2\imgproc\imgproc.hpp"

/* USING OpenCV */

int asdmain(int argc, char* argv[])
{
	char input_file[128];
	char output_file[64];
	
	if (argc == 4) {
		strcpy(input_file, argv[1]);
		strcpy(output_file, argv[2]);
	}
	else {
		strcpy(input_file, "D:\\temp\\Charite\\Camelyon16\\Tumor\\Tumor_001.tif");
		strcpy(output_file, "Tumor_001.jp2");
		//level = 3;
	}

	openslide_t* slide = openslide_open(input_file);
	//openslide_t* mask = openslide_open("D:\\temp\\Charite\\Camelyon16\\Tumor\\Mask\\Tumor_001_Mask.tif");

	int32_t levCount = openslide_get_level_count(slide);

	//int32_t level = (openslide_get_level_count(mask) < openslide_get_level_count(slide)) ? openslide_get_level_count(mask) - 1 : openslide_get_level_count(slide) - 1;
	int32_t level = 0;

	//level = ;
	//level = 5;

	double *ds = new double[levCount];
	for (int i = 0; i < levCount; i++) {
		ds[i] = openslide_get_level_downsample(slide, i);
		std::cout << "ds[" << i << "]  = " << ds[i] << ", " << 4096 * ds[0] / ds[i] << std::endl;
	}

	int64_t x = 30608, y = 111120, width, height;
	int64_t size = 4096 * ds[0] / ds[level];
	size_t tileSize = size * size * 4;
	uint32_t *tile = (uint32_t*)malloc(tileSize);

	for (;;) {

		free(tile);

		openslide_get_level_dimensions(slide, 0, &width, &height);

		//width = 512; height = 512;		
		size = 4096 * ds[0] / ds[level];

		//std::cout << "was: \nH = " << height << ", W = " << width << std::endl;

		width = (int64_t)ceil((double)width / (double)size) * (int64_t)size;
		height = (int64_t)ceil((double)height / (double)size) * (int64_t)size;

		//std::cout << "now: \nH = " << height << ", W = " << width << std::endl;

		tileSize = size * size * 4;
		tile = (uint32_t*)malloc(tileSize);
		//size_t tileSize = width * height * 4;
		//tile = (uint32_t*)malloc(tileSize);

		openslide_read_region(slide, tile, x, y, level, size, size);
		//openslide_read_region(slide, tile, x, y, 0, width, height);

	//	cv::Mat img(size, size, CV_8UC4, tile);
		cv::Mat img(size, size, CV_8UC4);

		OPJ_UINT32 t = 0;
		for (int i = 0; i < size; i++) {
			for (int j = 0; j < size; j++) {
				
				int32_t m_pixel = tile[t++];

				// convert it to bytes (4 bytes in 1 pixel, i.e. R, G, B and A)
				OPJ_BYTE
					B_bit = (m_pixel & 0x000000ff),
					G_bit = (m_pixel & 0x0000ff00) >> 8,
					R_bit = (m_pixel & 0x00ff0000) >> 16,
					A_bit = (m_pixel & 0xff000000) >> 24;

				img.at<cv::Vec4b>(i, j)[0] = (uchar)B_bit;
				img.at<cv::Vec4b>(i, j)[1] = (uchar)G_bit;
				img.at<cv::Vec4b>(i, j)[2] = (uchar)R_bit;
				img.at<cv::Vec4b>(i, j)[3] = (uchar)A_bit;
			}
		}

		//cv::Mat img(height, width, CV_8UC4, tile);
		cv::Mat img2;
		img.copyTo(img2);


		cv::Mat bgra[4];   //destination array
		cv::split(img2, bgra);//split source 

		cv::imshow("image", bgra[3]);

		//std::cout << "tileSize = " << tileSize << ", size*size = " << size * size << std::endl;

		int c = cv::waitKey(10);

		;
		if (27 == char(c))
			break;
		else if (char(c) == 'd') {
			x += size;
			//if (x > width) x = width - size - 1;

			std::cout << "x = " << x << ", y = " << y << std::endl;
		}
		else if (char(c) == 's') {
			y += size;
			//	if (y > height) y = height - size - 1;

			std::cout << "x = " << x << ", y = " << y << std::endl;
		}
		else if (char(c) == 'w') {
			y -= size;
			if (y < 0) y = 0;

			std::cout << "x = " << x << ", y = " << y << std::endl;
		}
		else if (char(c) == 'a') {
			x -= size;
			if (x < 0) x = 0;

			std::cout << "x = " << x << ", y = " << y << std::endl;
		}
		else if (char(c) == 'q') {
			level--;
			if (level < 0) level = 0;
		}
		else if (char(c) == 'e') {
			level++;
			if (level > levCount - 1) level = levCount - 1;
		}
		else if (char(c) == 32) {
			cv::imwrite("iamge.png", img2);

			int32_t pixel = tile[size * size - 2];

			// convert it to bytes (4 bytes in 1 pixel, i.e. R, G, B and A)
			OPJ_BYTE
				B_bit = (pixel & 0x000000ff),
				G_bit = (pixel & 0x0000ff00) >> 8,
				R_bit = (pixel & 0x00ff0000) >> 16,
				A_bit = (pixel & 0xff000000) >> 24;

			std::cout << "RGBA: (" << (int)R_bit << "; " << (int)G_bit << "; " << (int)B_bit << "; " << (int)A_bit << ")" << std::endl;

			std::ofstream fout;
			fout.open("file.txt", std::ios::out);

			for (int64_t i = 0; i < size*size; i++) {
				int32_t pixel = tile[i];
				//fout << tile[i] << "; ";
				B_bit = (pixel & 0x000000ff);
				G_bit = (pixel & 0x0000ff00) >> 8;
				R_bit = (pixel & 0x00ff0000) >> 16;
				A_bit = (pixel & 0xff000000) >> 24;

				fout << "RGBA: (" << (int)R_bit << "; " << (int)G_bit << "; " << (int)B_bit << "; " << (int)A_bit << ")" << std::endl;
			}

			fout.close();
		}
		else
			continue;

	}

	openslide_close(slide);
	

	return 0;
}

