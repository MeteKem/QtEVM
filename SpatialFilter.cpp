// Yet anther C++ implementation of EVM, based on OpenCV and Qt. 
// Copyright (C) 2014  Joseph Pan <cs.wzpan@gmail.com>
// 
// This library is free software; you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as
// published by the Free Software Foundation; either version 2.1 of the
// License, or (at your option) any later version.
// 
// This library is distributed in the hope that it will be useful, but
// WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// Lesser General Public License for more details.
// 
// You should have received a copy of the GNU Lesser General Public
// License along with this library; if not, write to the Free Software
// Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
// 02110-1301 USA
// 

#include "SpatialFilter.h"

/** 
 * buildLaplacianPyramid	-	construct a laplacian pyramid from given image
 *
 * @param img		-	source image
 * @param levels	-	levels of the destinate pyramids
 * @param pyramid	-	destinate image
 *
 * @return true if success
 */
bool buildLaplacianPyramid(const cv::Mat &img, const int levels,
                           std::vector<cv::Mat> &pyramid)
{
    if (levels < 1){
        perror("Levels should be larger than 1");
        return false;
    }
    pyramid.clear();
    cv::Mat currentImg = img;
    cv::gpu::GpuMat d_currentImg;
    d_currentImg.upload(currentImg);
    for (int l=0; l<levels; l++) {
        cv::gpu::GpuMat down,up;
        cv::gpu::pyrDown(d_currentImg, down);
        cv::gpu::pyrUp(down, up);
        cv::gpu::GpuMat d_lap;
        cv::gpu::subtract(d_currentImg, up, d_lap);
        cv::Mat lap(d_lap);
        pyramid.push_back(lap);
        d_currentImg = down;
    }
    d_currentImg.download(currentImg);
    pyramid.push_back(currentImg);
    return true;
}

/** 
 * buildGaussianPyramid	-	construct a gaussian pyramid from a given image
 *
 * @param img		-	source image
 * @param levels	-	levels of the destinate pyramids
 * @param pyramid	-	destinate image
 *
 * @return true if success
 */
bool buildGaussianPyramid(const cv::Mat &img,
                          const int levels,
                          std::vector<cv::Mat> &pyramid)
{
    if (levels < 1){
        perror("Levels should be larger than 1");
        return false;
    }
    pyramid.clear();
    cv::Mat currentImg = img;
    cv::gpu::GpuMat d_currentImg(currentImg);
    for (int l=0; l<levels; l++) {
        cv::gpu::GpuMat down;
        cv::gpu::pyrDown(d_currentImg, down);
        cv::Mat matDown;
        down.upload(matDown);
        pyramid.push_back(matDown);
        d_currentImg = down;
    }
    d_currentImg.download(currentImg);
    return true;
}

/** 
 * reconImgFromLaplacianPyramid	-	reconstruct image from given laplacian pyramid
 *
 * @param pyramid	-	source laplacian pyramid
 * @param levels	-	levels of the pyramid
 * @param dst		-	destinate image
 */
void reconImgFromLaplacianPyramid(const std::vector<cv::Mat> &pyramid,
                                  const int levels,
                                  cv::Mat &dst)
{
    cv::Mat currentImg = pyramid[levels];
    cv::gpu::GpuMat d_currentImg(currentImg);
    for (int l=levels-1; l>=0; l--) {
        cv::gpu::GpuMat up;
        cv::gpu::pyrUp(d_currentImg, up);
        
        cv::gpu::GpuMat pyrTemp(pyramid[l]);
        cv::gpu::add(up, pyrTemp, d_currentImg);
    }
    d_currentImg.download(dst);
    //if the line above causes errors, try the two lines below instead
//    d_currentImg.download(currentImg);
//    dst = currentImg.clone();
}

/** 
 * upsamplingFromGaussianPyramid	-	up-sampling an image from gaussian pyramid
 *
 * @param src		-	source image
 * @param levels	-	levels of the pyramid
 * @param dst		-	destinate image
 */
void upsamplingFromGaussianPyramid(const cv::Mat &src,
                                   const int levels,
                                   cv::Mat &dst)
{
    cv::Mat currentLevel = src.clone();
    cv::gpu::GpuMat d_currentLevel(currentLevel);
    for (int i = 0; i < levels; ++i) {
        cv::gpu::GpuMat up;
        cv::gpu::pyrUp(d_currentLevel, up);
        d_currentLevel = up;
    }
    d_currentLevel.download(dst);
}
