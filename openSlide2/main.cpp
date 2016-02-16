/*
* Copyright (c) 2008, Jerome Fimes, Communications & Systemes <jerome.fimes@c-s.fr>
* All rights reserved.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions
* are met:
* 1. Redistributions of source code must retain the above copyright
*    notice, this list of conditions and the following disclaimer.
* 2. Redistributions in binary form must reproduce the above copyright
*    notice, this list of conditions and the following disclaimer in the
*    documentation and/or other materials provided with the distribution.
*
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS `AS IS'
* AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
* IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
* ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
* LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
* CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
* SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
* INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
* CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
* ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
* POSSIBILITY OF SUCH DAMAGE.
*/



#include "stdafx.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

#include "stdlib.h"

#include "openjpeg.h"
#include "opj_stdint.h"

#include "openslide\openslide.h"


/* -------------------------------------------------------------------------- */

/**
sample error debug callback expecting no client object
*/
static void error_callback(const char *msg, void *client_data) {
	(void)client_data;
	fprintf(stdout, "[ERROR] %s", msg);
}
/**
sample warning debug callback expecting no client object
*/
static void warning_callback(const char *msg, void *client_data) {
	(void)client_data;
	fprintf(stdout, "[WARNING] %s", msg);
}
/**
sample debug callback expecting no client object
*/
static void info_callback(const char *msg, void *client_data) {
	(void)client_data;
	fprintf(stdout, "[INFO] %s", msg);
}

/* -------------------------------------------------------------------------- */

#define NUM_COMPS_MAX 4
int main(int argc, char *argv[])
{
	/* FIRST EXTRACT SOME INFORMATION ABOUT THE SLIDES */
	openslide_t* slide = openslide_open("D:\\temp\\Charite\\Camelyon16\\Tumor\\Tumor_002.tif");
	openslide_t* mask = openslide_open("D:\\temp\\Charite\\Camelyon16\\Tumor\\Mask\\Tumor_002_Mask.tif");

	// get the amount of layers
	int32_t levCount = openslide_get_level_count(slide);

	// get the highest level (with the lowest resolution)
	int32_t level = (openslide_get_level_count(mask) < openslide_get_level_count(slide)) ? openslide_get_level_count(mask) - 1 : openslide_get_level_count(slide) - 1;

	// set it to the lowest layer (the true resoluion, i.e. in Gigapixels)
	level = 0;

	// get the downsample rate for every layer
	double *ds = new double[levCount];
	for (int i = 0; i < levCount; i++) {
		ds[i] = openslide_get_level_downsample(slide, i);
	}

	int64_t x = 0, y = 0, width, height;

	// get the dimensions for the current layer. Currently we just take the true dimensions
	openslide_get_level_dimensions(slide, level, &width, &height);

	// get tile width/height at current level
	int64_t size = 4096 * ds[0] / ds[level];
	size = 512;

	fprintf(stdout, "\nPrinting information\nW = %d\nH = %d\nSize = %d\n", width, height, size);

	// get full tile size at current level
	size_t tileSize = size * size * 4;

	// prepare a buffer for the tile
	uint32_t *tile = NULL;
	tile = (uint32_t*)malloc(tileSize);

	/*
	* Now switching to the OpenJPEG part
	* I didn't change much here, just took an example from the official github repo
	* which can be found here: https://github.com/uclouvain/openjpeg/blob/master/tests/test_tile_encoder.c
	*/
	opj_cparameters_t l_param;
	opj_codec_t * l_codec;
	opj_image_t * l_image;
	opj_image_cmptparm_t l_params[NUM_COMPS_MAX];
	opj_stream_t * l_stream;
	OPJ_UINT32 l_nb_tiles, l_nb_tiles_x, l_nb_tiles_y;
	OPJ_UINT32 l_data_size;
	unsigned char len;

#ifdef USING_MCT
	const OPJ_FLOAT32 l_mct[] =
	{
		1, 0, 0,
		0, 1, 0,
		0, 0, 1
	};

	const OPJ_INT32 l_offsets[] =
	{
		128, 128, 128
	};
#endif

	opj_image_cmptparm_t * l_current_param_ptr;
	OPJ_UINT32 i;
	OPJ_BYTE *l_data;

	OPJ_UINT32 num_comps;
	int image_width;
	int image_height;
	int tile_width;
	int tile_height;
	int comp_prec;
	int irreversible;
	char output_file[64];

	// set image data, i.e. dimensions, tile size, components, name
	num_comps = 4;
	image_width = width;
	image_height = height;
	tile_width = size;
	tile_height = size;
	comp_prec = 8;
	irreversible = 1;

	//fprintf(stdout, "tiles: %d x %d\n", tile_width, tile_height);

	// name of output image
	// TODO: make it so that the output name is automatically derived from the input
	strcpy(output_file, "Tumor_002.jp2");

	// check if the current number of components is within limits (cannot be higher than 4)
	if (num_comps > NUM_COMPS_MAX)
	{
		return 1;
	}

	// get number of tiles vertically and horizontally
	l_nb_tiles_x = (OPJ_UINT32)(image_width / tile_width);
	l_nb_tiles_y = (OPJ_UINT32)(image_height / tile_height);

	// total number of tiles
	l_nb_tiles = (OPJ_UINT32)(l_nb_tiles_x * l_nb_tiles_y);

	l_data_size = (OPJ_UINT32)tile_width * (OPJ_UINT32)tile_height * (OPJ_UINT32)num_comps * (OPJ_UINT32)(comp_prec / 8);

	l_data = (OPJ_BYTE*)malloc(l_data_size * sizeof(OPJ_BYTE));

	OPJ_UINT32 offset = (OPJ_UINT32)l_data_size / (OPJ_UINT32)num_comps;

	opj_set_default_encoder_parameters(&l_param);
	/** you may here add custom encoding parameters */
	/* rate specifications */
	/** number of quality layers in the stream */
	l_param.tcp_numlayers = levCount;
	l_param.cp_fixed_quality = 1;
	l_param.tcp_distoratio[0] = 20;
	/* is using others way of calculation */
	/* l_param.cp_disto_alloc = 1 or l_param.cp_fixed_alloc = 1 */
	/* l_param.tcp_rates[0] = ... */


	/* tile definitions parameters */
	/* position of the tile grid aligned with the image */
	l_param.cp_tx0 = 0;
	l_param.cp_ty0 = 0;
	/* tile size, we are using tile based encoding */
	l_param.tile_size_on = OPJ_TRUE;
	l_param.cp_tdx = tile_width;
	l_param.cp_tdy = tile_height;

	/* use irreversible encoding ?*/
	l_param.irreversible = irreversible;

	/* do not bother with mct, the rsiz is set when calling opj_set_MCT*/
	/*l_param.cp_rsiz = OPJ_STD_RSIZ;*/

	/* no cinema */
	/*l_param.cp_cinema = 0;*/

	/* no not bother using SOP or EPH markers, do not use custom size precinct */
	/* number of precincts to specify */
	/* l_param.csty = 0;*/
	/* l_param.res_spec = ... */
	/* l_param.prch_init[i] = .. */
	/* l_param.prcw_init[i] = .. */


	/* do not use progression order changes */
	/*l_param.numpocs = 0;*/
	/* l_param.POC[i].... */

	/* do not restrain the size for a component.*/
	/* l_param.max_comp_size = 0; */

	/** block encoding style for each component, do not use at the moment */
	/** J2K_CCP_CBLKSTY_TERMALL, J2K_CCP_CBLKSTY_LAZY, J2K_CCP_CBLKSTY_VSC, J2K_CCP_CBLKSTY_SEGSYM, J2K_CCP_CBLKSTY_RESET */
	/* l_param.mode = 0;*/

	/** number of resolutions */
	l_param.numresolution = 6;

	/** progression order to use*/
	/** OPJ_LRCP, OPJ_RLCP, OPJ_RPCL, PCRL, CPRL */
	l_param.prog_order = OPJ_LRCP;

	/** no "region" of interest, more precisally component */
	/* l_param.roi_compno = -1; */
	/* l_param.roi_shift = 0; */

	/* we are not using multiple tile parts for a tile. */
	/* l_param.tp_on = 0; */
	/* l_param.tp_flag = 0; */

	/* if we are using mct */
#ifdef USING_MCT
	opj_set_MCT(&l_param, l_mct, l_offsets, NUM_COMPS);
#endif

	/* image definition */
	l_current_param_ptr = l_params;
	for (i = 0; i<num_comps; ++i) {
		/* do not bother bpp useless */
		/*l_current_param_ptr->bpp = COMP_PREC;*/
		l_current_param_ptr->dx = 1;
		l_current_param_ptr->dy = 1;

		l_current_param_ptr->h = (OPJ_UINT32)image_height;
		l_current_param_ptr->w = (OPJ_UINT32)image_width;

		l_current_param_ptr->sgnd = 0;
		l_current_param_ptr->prec = (OPJ_UINT32)comp_prec;

		l_current_param_ptr->x0 = 0;
		l_current_param_ptr->y0 = 0;

		++l_current_param_ptr;
	}

	/* should we do j2k or jp2 ?*/
	len = (unsigned char)strlen(output_file);
	if (strcmp(output_file + len - 4, ".jp2") == 0)
	{
		l_codec = opj_create_compress(OPJ_CODEC_JP2);
	}
	else
	{
		l_codec = opj_create_compress(OPJ_CODEC_J2K);
	}
	if (!l_codec) {
		return 1;
	}

	/* catch events using our callbacks and give a local context */
	opj_set_info_handler(l_codec, info_callback, 00);
	opj_set_warning_handler(l_codec, warning_callback, 00);
	opj_set_error_handler(l_codec, error_callback, 00);

	l_image = opj_image_tile_create(num_comps, l_params, OPJ_CLRSPC_SRGB);
	if (!l_image) {
		opj_destroy_codec(l_codec);
		return 1;
	}

	l_image->x0 = 0;
	l_image->y0 = 0;
	l_image->x1 = (OPJ_UINT32)image_width;
	l_image->y1 = (OPJ_UINT32)image_height;
	l_image->color_space = OPJ_CLRSPC_SRGB;

	if (!opj_setup_encoder(l_codec, &l_param, l_image)) {
		fprintf(stderr, "ERROR -> test_tile_encoder: failed to setup the codec!\n");
		opj_destroy_codec(l_codec);
		opj_image_destroy(l_image);

		openslide_close(slide);
		openslide_close(mask);

		return 1;
	}

	l_stream = opj_stream_create_default_file_stream(output_file, OPJ_FALSE);
	if (!l_stream) {
		fprintf(stderr, "ERROR -> test_tile_encoder: failed to create the stream from the output file %s !\n", output_file);
		opj_destroy_codec(l_codec);
		opj_image_destroy(l_image);

		openslide_close(slide);
		openslide_close(mask);

		return 1;
	}

	if (!opj_start_compress(l_codec, l_image, l_stream)) {
		fprintf(stderr, "ERROR -> test_tile_encoder: failed to start compress!\n");
		opj_stream_destroy(l_stream);
		opj_destroy_codec(l_codec);
		opj_image_destroy(l_image);

		openslide_close(slide);
		openslide_close(mask);

		return 1;
	}

	fprintf(stdout, "\nPrinting information\nl_nb_tiles_x = %d\nl_nb_tiles_y = %d\nl_nb_tiles = %d\n", l_nb_tiles_x, l_nb_tiles_y, l_nb_tiles);

	OPJ_INT32 tNum = 0;
	for (int x = 0; x < l_nb_tiles_x; ++x) {
		for (int y = 0; y < l_nb_tiles_y; ++y) {

		//	fprintf(stdout, "\ntile coord [%d ; %d], tile num = %d\n", x * size, y * size, tNum);

			// read tile
			openslide_read_region(slide, tile, x * size, y * size, level, size, size);

			//fprintf(stdout, "just to check, tilesize = %d, tilesize * 4 = %d, and l_data_size = %d\n", tileSize, tileSize * 4, l_data_size);

			int i = 0;
			for (OPJ_INT32 j = 0, i = 0; j < offset; ++j) {
				// take one pixel
				int32_t pixel = tile[j];

				// convert it to bytes (4 bytes in 1 pixel, i.e. R, G, B and A)
				OPJ_BYTE
					R_bit = (pixel & 0x000000ff),
					G_bit = (pixel & 0x0000ff00) >> 8,
					B_bit = (pixel & 0x00ff0000) >> 16,
					A_bit = (pixel & 0xff000000) >> 24;

				// write the bytes into the data stream
				l_data[j]								= R_bit;
				l_data[j + 1 * offset]	= G_bit;
				l_data[j + 2 * offset]	= B_bit;
				l_data[j + 3 * offset]	= A_bit;
				//l_data[j] = (OPJ_BYTE)pixel;

				//i += num_comps;
			}

			// write the tile into the file
			if (!opj_write_tile(l_codec, tNum, l_data, l_data_size, l_stream)) {
				fprintf(stderr, "ERROR -> test_tile_encoder: failed to write the tile %d!\n", i);
				opj_stream_destroy(l_stream);
				opj_destroy_codec(l_codec);
				opj_image_destroy(l_image);

				openslide_close(slide);
				openslide_close(mask);

				return 1;
			}

			tNum++;
		}
	}

	if (!opj_end_compress(l_codec, l_stream)) {
		fprintf(stderr, "ERROR -> test_tile_encoder: failed to end compress!\n");
		opj_stream_destroy(l_stream);
		opj_destroy_codec(l_codec);
		opj_image_destroy(l_image);

		openslide_close(slide);
		openslide_close(mask);

		return 1;
	}

	opj_stream_destroy(l_stream);
	opj_destroy_codec(l_codec);
	opj_image_destroy(l_image);

	free(l_data);

	openslide_close(slide);
	openslide_close(mask);

	/* Print profiling*/
	/*PROFPRINT();*/

	return 0;
}





//// OpenSlideExtractor1.cpp : Defines the entry point for the console application.
////
//
//#include "stdafx.h"
//
//#include <stdlib.h>
//#include <stdint.h>
//#include <limits.h>
//#include <assert.h>
//
//#include "openslide\openslide.h"
//
//#include "openjpeg.h"
//#include "opj_stdint.h"
//
//#include "opencv2\core\core.hpp"
//#include "opencv2\highgui\highgui.hpp"
//#include "opencv2\imgproc\imgproc.hpp"
//
///* USING OpenCV */
//
//int main(int argc, char* argv[])
//{
//	openslide_t* slide = openslide_open("D:\\temp\\Charite\\Camelyon16\\Tumor\\Tumor_001.tif");
//	openslide_t* mask = openslide_open("D:\\temp\\Charite\\Camelyon16\\Tumor\\Mask\\Tumor_001_Mask.tif");
//
//	int32_t levCount = openslide_get_level_count(slide);
//
//	int32_t level = (openslide_get_level_count(mask) < openslide_get_level_count(slide)) ? openslide_get_level_count(mask) - 1 : openslide_get_level_count(slide) - 1;
//
//	level = 0;
//
//	double *ds = new double[levCount];
//	for (int i = 0; i < levCount; i++) {
//		ds[i] = openslide_get_level_downsample(slide, i);
//		std::cout << "ds[" << i << "]  = " << ds[i] << ", " << 4096 * ds[0] / ds[i] << std::endl;
//	}
//
//	int64_t x = 0, y = 0, width, height;
//	int64_t size = 4096 * ds[0] / ds[level];
//	size = 512;
//	size_t tileSize = size * size * 4;
//	uint32_t *tile = (uint32_t*)malloc(tileSize);
//
//	for (;;) {
//
//		free(tile);
//
//		openslide_get_level_dimensions(slide, 0, &width, &height);
//		//width = 512; height = 512;		
//		//size = 4096 * ds[0] / ds[level];
//		
//		// tileSize = size * size * 4;
//		tile = (uint32_t*)malloc(tileSize);
//		//size_t tileSize = width * height * 4;
//		//tile = (uint32_t*)malloc(tileSize);
//
//		openslide_read_region(slide, tile, x, y, level, size, size);
//		//openslide_read_region(slide, tile, x, y, 0, width, height);
//
//		cv::Mat img(size, size, CV_8UC4, tile);
//		//cv::Mat img(height, width, CV_8UC4, tile);
//		cv::Mat img2;
//		img.copyTo(img2);
//
//		cv::imshow("image", img2);
//
//		int c = cv::waitKey(10);
//
//		if (27 == char(c))
//			break;
//		else if (char(c) == 'd') {
//			x += size;
//			if (x > width) x = width - size - 1;
//
//			std::cout << "x = " << x << ", y = " << y << std::endl;
//		}
//		else if (char(c) == 's') {
//			y += size;
//			if (y > height) y = height - size - 1;
//
//			std::cout << "x = " << x << ", y = " << y << std::endl;
//		}
//		else if (char(c) == 'w') {
//			y -= size;
//			if (y < 0) y = 0;
//
//			std::cout << "x = " << x << ", y = " << y << std::endl;
//		}
//		else if (char(c) == 'a') {
//			x -= size;
//			if (x < 0) x = 0;
//
//			std::cout << "x = " << x << ", y = " << y << std::endl;
//		}
//		else if (char(c) == 'q') {
//			level--;
//			if (level < 0) level = 0;
//		}
//		else if (char(c) == 'e') {
//			level++;
//			if (level > levCount - 1) level = levCount - 1;
//		}
//		else
//			continue;
//
//	}
//
//	openslide_close(slide);
//	openslide_close(mask);
//
//	//max_level -= 3;
//	/*
//	int64_t width, height;
//	openslide_get_level_dimensions(slide, max_level, &width, &height);
//
//	size_t imgSize = width * height * 4;
//
//	uint32_t *imgData = (uint32_t*)malloc(imgSize);
//	std::vector<uint32_t> dst(width*height*4);
//	openslide_read_region(slide, imgData, 0, 0, max_level, width, height);
//	openslide_read_region(slide, &dst[0], 0, 0, max_level, width, height);
//
//	for (size_t i = 600; i < 650; i++)
//	{
//	int32_t pixel = imgData[i];
//
//	unsigned char byte1 = pixel & 0x000000ff,
//	byte2 = (pixel & 0x0000ff00) >> 8,
//	byte3 = (pixel & 0x00ff0000) >> 16,
//	byte4 = (pixel & 0xff000000) >> 24;
//
//	//std::cout << (int)byte1 << "," << (int)byte2 << "," << (int)byte3 << "," << (int)byte4 << std::endl;
//	}
//
//
//	cv::Mat img(height, width, CV_8UC4, imgData);
//	cv::Mat img2;
//	img.copyTo(img2);
//
//	cv::Mat img3;
//	cv::cvtColor(img2, img3, CV_RGBA2BGR);
//
//	//cv::imwrite("C:\\Users\\kovalenm\\Work\\imgs\\Tumor_001_4.jp2", img3);
//
//	for (;;) {
//	cv::imshow("image", img2);
//
//	int c = cv::waitKey(10);
//
//	if (27 == char(c))
//	break;
//	}
//
//	openslide_close(slide);
//	openslide_close(mask);
//
//	*/
//
//	return 0;
//}
//
