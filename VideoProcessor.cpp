#include "VideoProcessor.h"

VideoProcessor::VideoProcessor(QObject *parent)
  : QObject(parent)
  , delay(-1)
  , rate(0)
  , fnumber(0)
  , length(0)
  , stop(true)
  , modify(false)
  , curPos(0)
  , curIndex(0)
  , curLevel(0)
  , digits(0)
  , extension(".avi")
  , levels(4)
  , alpha(10)
  , lambda_c(16)
  , fl(0.05)
  , fh(0.4)
  , chromAttenuation(0.1)
  , delta(0)
  , exaggeration_factor(0.2)
  , lambda(0)
{
    connect(this, SIGNAL(revert()), this, SLOT(revertVideo()));
}

/** 
 * setDelay	-	 set a delay between each frame
 *
 * 0 means wait at each frame, 
 * negative means no delay
 * @param d	-	delay param
 */
void VideoProcessor::setDelay(int d)
{
    delay = d;
}

/** 
 * getNumberOfProcessedFrames	-	a count is kept of the processed frames
 *
 *
 * @return the number of processed frames
 */
long VideoProcessor::getNumberOfProcessedFrames()
{
    return fnumber;
}

/** 
 * getNumberOfPlayedFrames	-	get the current playing progress
 *
 * @return the number of played frames
 */
long VideoProcessor::getNumberOfPlayedFrames()
{
    return curPos;
}

/** 
 * getFrameSize	-	return the size of the video frame
 *
 *
 * @return the size of the video frame
 */
cv::Size VideoProcessor::getFrameSize()
{
    int w = static_cast<int>(capture.get(CV_CAP_PROP_FRAME_WIDTH));
    int h = static_cast<int>(capture.get(CV_CAP_PROP_FRAME_HEIGHT));

    return cv::Size(w,h);
}

/** 
 * getFrameNumber	-	return the frame number of the next frame
 *
 *
 * @return the frame number of the next frame
 */
long VideoProcessor::getFrameNumber()
{
    long f = static_cast<long>(capture.get(CV_CAP_PROP_POS_FRAMES));

    return f;
}

/** 
 * getPositionMS	-	return the position in milliseconds
 *
 * @return the position in milliseconds
 */
double VideoProcessor::getPositionMS()
{
    double t = capture.get(CV_CAP_PROP_POS_MSEC);

    return t;
}

/** 
 * getFrameRate	-	return the frame rate
 *
 *
 * @return the frame rate
 */
double VideoProcessor::getFrameRate()
{
    double r = capture.get(CV_CAP_PROP_FPS);

    return r;
}

/** 
 * getLength	-	return the number of frames in video
 *
 * @return the number of frames
 */
long VideoProcessor::getLength()
{
    return length;
}


/** 
 * getLengthMS	-	return the video length in milliseconds
 *
 *
 * @return the length of length in milliseconds
 */
double VideoProcessor::getLengthMS()
{
    double l = 1000.0 * length / rate;
    return l;
}

/** 
 * calculateLength	-	recalculate the number of frames in video
 * 
 * normally doesn't need it unless getLength()
 * can't return a valid value
 */
void VideoProcessor::calculateLength()
{
    long l = 0;
    cv::Mat img;
    cv::VideoCapture tempCapture(tempFile);
    while(tempCapture.read(img)){
        ++l;
    }
    length = l;
    tempCapture.release();
}

/** 
 * spatialFilter	-	spatial filtering an image
 *
 * @param src		-	source image 
 * @param pyramid	-	destinate pyramid
 */
bool VideoProcessor::spatialFilter(const cv::Mat &src, std::vector<cv::Mat_<cv::Vec3f> > &pyramid)
{
    switch (spatialType) {
    case LAPLACIAN:     // laplacian pyramid
        return buildLaplacianPyramid(src, levels, pyramid);
        break;
    case GAUSSIAN:      // gaussian pyramid
        return buildGaussianPyramid(src, levels, pyramid);
        break;
    default:
        return false;
        break;
    }
}

/** 
 * temporalFilter	-	temporal filtering an image
 *
 * @param src	-	source image
 * @param dst	-	destinate image
 */
void VideoProcessor::temporalFilter(const cv::Mat_<cv::Vec3f> &src,
                                    cv::Mat_<cv::Vec3f> &dst)
{
    switch(temporalType) {
    case IIR:       // IIR bandpass filter
        temporalIIRFilter(src, dst);
        break;
    case IDEAL:
        temporalIdealFilter(src, dst);
        break;
    default:        
        break;
    }
    return;
}

/** 
 * temporalIIRFilter	-	temporal IIR filtering an image
 *
 * @param pyramid	-	source image
 * @param filtered	-	filtered result
 *
 * @return true if amplifying is allowed; false otherwise
 */
void VideoProcessor::temporalIIRFilter(const cv::Mat_<cv::Vec3f> &src,
                                    cv::Mat_<cv::Vec3f> &dst)
{
    cv::Mat temp1 = (1-fh)*lowpass1[curLevel] + fh*src;
    cv::Mat temp2 = (1-fl)*lowpass2[curLevel] + fl*src;
    lowpass1[curLevel] = temp1;
    lowpass2[curLevel] = temp2;
    dst = lowpass1[curLevel] - lowpass2[curLevel];
}

/** 
 * temporalIdalFilter	-	temporal IIR filtering an image pyramid of concat-frames
 *
 * @param pyramid	-	source pyramid of concatenate frames
 * @param filtered	-	concatenate filtered result
 *
 * @return true if amplifying is allowed; false otherwise
 */
void VideoProcessor:: temporalIdealFilter(const cv::Mat_<cv::Vec3f> &src,
                                          cv::Mat_<cv::Vec3f> &dst)
{
    cv::Mat colors[3], planes[2];
    cv::Mat padded, complexImg, filter;

    // split into 3 channels
    cv::split(src, colors);

    for (int j = 0; j < 3; ++j){
        int M = cv::getOptimalDFTSize(src.rows);
        int N = cv::getOptimalDFTSize(src.cols);
        cv::Mat temp = colors[j];
        // setup the DFT images
        cv::copyMakeBorder(temp, padded, 0, M - temp.rows, 0,
                           N - temp.cols, cv::BORDER_CONSTANT, cv::Scalar::all(0));
        planes[0] = cv::Mat_<float>(padded);
        planes[1] = cv::Mat::zeros(padded.size(), CV_32F);

        cv::merge(planes, 2, complexImg);

        // do the DFT
        cv::dft(complexImg, complexImg, cv::DFT_ROWS | cv::DFT_SCALE);

        // construct the filter
        filter = complexImg.clone();
        createIdealBandpassFilter(filter, fl, fh, rate);

        // apply filter
        shiftDFT(complexImg);
        cv::mulSpectrums(complexImg, filter, complexImg, 0);
        shiftDFT(complexImg);

        // do the IDFT
        cv::idft(complexImg, complexImg, cv::DFT_ROWS | cv::DFT_SCALE);

        // split into planes and extract plane 0 as output image
        cv::Mat myplanes[2];
        split(complexImg, myplanes);
        double minimum = -1;
        double maximum = -1;
        cv::Point minloc(-1, -1), maxloc(-1, -1);
        cv::minMaxLoc(myplanes[0], &minimum, &maximum, &minloc, &maxloc);
        // cv::normalize(myplanes[0], temp, 0, 1, CV_MINMAX);
        myplanes[0].copyTo(temp);
    }
    cv::merge(colors, 3, dst);
}

/** 
 * amplify	-	ampilfy the motion
 *
 * @param filtered	- motion image
 */
void VideoProcessor::amplify(cv::Mat_<cv::Vec3f> &image)
{
    float currAlpha;
    switch (spatialType) {
    case LAPLACIAN:
        //compute modified alpha for this level
        currAlpha = lambda/delta/8 - 1;
        currAlpha *= exaggeration_factor;
        if (curLevel==levels-1 || curLevel==0)     // ignore the highest and lowest frequency band
            image *= 0;
        else if (currAlpha > alpha)  // representative lambda exceeds lambda_c
            image *= alpha;
        else
            image *= currAlpha;
        break;
    case GAUSSIAN:
        image *= alpha;
        break;
    default:
        break;
    }
}

void VideoProcessor::combineImageAndMotion(const cv::Mat src,
                                         const std::vector<cv::Mat_<cv::Vec3f> > &filtered,
                                         cv::Mat &dst)
{
    cv::Mat_<cv::Vec3f> motion;
    switch (spatialType) {
    case LAPLACIAN:
        reconImgFromLaplacianPyramid(filtered, levels, motion);
        attenuate(motion);
        dst = src + motion;
        break;
    case GAUSSIAN:        
        break;
    default:
        break;
    }
    motion.release();
}

void VideoProcessor::attenuate(cv::Mat_<cv::Vec3f> &image)
{
    cv::Mat planes[3];
    cv::split(image, planes);
    planes[1] = planes[1] * chromAttenuation;
    planes[2] = planes[2] * chromAttenuation;
    cv::merge(planes, 3, image);
}

/** 
 * getCodec	-	get the codec of input video
 *
 * @param codec	-	the codec arrays
 *
 * @return the codec integer
 */
int VideoProcessor::getCodec(char codec[])
{
    union {
        int value;
        char code[4]; } returned;

    returned.value = static_cast<int>(capture.get(CV_CAP_PROP_FOURCC));

    codec[0] = returned.code[0];
    codec[1] = returned.code[1];
    codec[2] = returned.code[2];
    codec[3] = returned.code[3];

    return returned.value;
}


/** 
 * getTempFile	-	temp file lists
 *
 * @param str	-	the reference of the output string
 */
void VideoProcessor::getTempFile(std::string &str)
{
    if (!tempFileList.empty()){
        str = tempFileList.back();
        tempFileList.pop_back();
    } else {
        str = "";
    }
}

/** 
 * getCurTempFile	-	get current temp file
 *
 * @param str	-	the reference of the output string
 */
void VideoProcessor::getCurTempFile(std::string &str)
{
    str = tempFile;
}

/** 
 * setInput	-	set the name of the expected video file
 *
 * @param fileName	-	the name of the video file
 *
 * @return True if success. False otherwise
 */
bool VideoProcessor::setInput(const std::string &fileName)
{
    fnumber = 0;
    tempFile = fileName;

    // In case a resource was already
    // associated with the VideoCapture instance
    if (isOpened()){
        capture.release();
    }

    // Open the video file
    if(capture.open(fileName)){
        // read parameters
        length = capture.get(CV_CAP_PROP_FRAME_COUNT);
        rate = getFrameRate();
        cv::Mat input;
        // show first frame
        getNextFrame(input);
        emit showFrame(input);
        emit updateBtn();
        return true;
    } else {
        return false;
    }
}

/** 
 * setOutput	-	set the output video file
 *
 * by default the same parameters than input video will be used
 *
 * @param filename	-	filename prefix
 * @param codec		-	the codec
 * @param framerate -	frame rate
 * @param isColor	-	is the video colorful
 *
 * @return True if successful. False otherwise
 */
bool VideoProcessor::setOutput(const std::string &filename, int codec, double framerate, bool isColor)
{
    outputFile = filename;
    extension.clear();

    if (framerate==0.0)
        framerate = getFrameRate(); // same as input

    char c[4];
    // use same codec as input
    if (codec==0) {
        codec = getCodec(c);
    }

    // Open output video
    return writer.open(outputFile, // filename
                       codec, // codec to be used
                       framerate,      // frame rate of the video
                       getFrameSize(), // frame size
                       isColor);       // color video?
}

/** 
 * set the output as a series of image files
 *
 * extension must be ".jpg", ".bmp" ...
 *
 * @param filename	-	filename prefix
 * @param ext		-	image file extension
 * @param numberOfDigits	-	number of digits
 * @param startIndex	-	start index
 *
 * @return True if successful. False otherwise
 */
bool VideoProcessor::setOutput(const std::string &filename, const std::string &ext, int numberOfDigits, int startIndex)
{
    // number of digits must be positive
    if (numberOfDigits<0)
        return false;

    // filenames and their common extension
    outputFile = filename;
    extension = ext;

    // number of digits in the file numbering scheme
    digits = numberOfDigits;
    // start numbering at this index
    curIndex = startIndex;

    return true;
}

/** 
 * setTemp	-	set the temp video file
 *
 * by default the same parameters to the input video
 *
 * @param codec	-	video codec
 * @param framerate	-	frame rate
 * @param isColor	-	is the video colorful
 *
 * @return True if successful. False otherwise
 */
bool VideoProcessor::createTemp(double framerate, bool isColor)
{
    std::stringstream ss;
    ss << "temp_" << QDateTime::currentDateTime().toTime_t() << ".avi";
    tempFile = ss.str();

    tempFileList.push_back(tempFile);

    if (framerate==0.0)
        framerate = getFrameRate(); // same as input

    // Open output video
    return tempWriter.open(tempFile, // filename
                       CV_FOURCC('M', 'J', 'P', 'G'), // codec to be used
                       framerate,      // frame rate of the video
                       getFrameSize(), // frame size
                       isColor);       // color video?
}

/** 
 * setSpatialFilter	-	set the spatial filter
 *
 * @param type	-	spatial filter type. Could be:
 *					1. LAPLACIAN: laplacian pyramid
 *					2. GAUSSIAN: gaussian pyramid
 */
void VideoProcessor::setSpatialFilter(spatialFilterType type)
{
    spatialType = type;
}

/** 
 * setTemporalFilter	-	set the temporal filter
 *
 * @param type	-	temporal filter type. Could be:
 *					1. IIR: second order(IIR) filter
 *					2. IDEAL: ideal bandpass filter
 */
void VideoProcessor::setTemporalFilter(temporalFilterType type)
{
    temporalType = type;
}

/** 
 * stopIt	-	stop playing or processing
 *
 */
void VideoProcessor::stopIt()
{
    stop = true;
    emit revert();
}

/** 
 * prevFrame	-	display the prev frame of the sequence
 *
 */
void VideoProcessor::prevFrame()
{
    if(isStop())
        pauseIt();
    if (curPos >= 0){
        curPos -= 1;
        jumpTo(curPos);
    }
    emit updateProgressBar();
}

/** 
 * nextFrame	-	display the next frame of the sequence
 *
 */
void VideoProcessor::nextFrame()
{
    if(isStop())
        pauseIt();
    curPos += 1;
    if (curPos <= length){
        curPos += 1;
        jumpTo(curPos);
    }
    emit updateProgressBar();
}

/** 
 * jumpTo	-	Jump to a position
 *
 * @param index	-	frame index
 *
 * @return True if success. False otherwise
 */
bool VideoProcessor::jumpTo(long index)
{
    if (index >= length){
        return 1;
    }

    cv::Mat frame;
    bool re = capture.set(CV_CAP_PROP_POS_FRAMES, index);

    if (re && !isStop()){
        capture.read(frame);
        emit showFrame(frame);
    }

    return re;
}


/** 
 * jumpToMS	-	jump to a position at a time
 *
 * @param pos	-	time
 *
 * @return True if success. False otherwise
 *
 */
bool VideoProcessor::jumpToMS(double pos)
{
    return capture.set(CV_CAP_PROP_POS_MSEC, pos);
}


/** 
 * close	-	close the video
 *
 */
void VideoProcessor::close()
{
    rate = 0;
    length = 0;
    modify = 0;
    capture.release();
    writer.release();
    tempWriter.release();
}


/** 
 * isStop	-	Is the processing stop
 *
 *
 * @return True if not processing/playing. False otherwise
 */
bool VideoProcessor::isStop()
{
    return stop;
}

/** 
 * isModified	-	Is the video modified?
 *
 *
 * @return True if modified. False otherwise
 */
bool VideoProcessor::isModified()
{
    return modify;
}

/** 
 * isOpened	-	Is the player opened?
 *
 *
 * @return True if opened. False otherwise 
 */
bool VideoProcessor::isOpened()
{
    return capture.isOpened();
}

/** 
 * getNextFrame	-	get the next frame if any
 *
 * @param frame	-	the expected frame
 *
 * @return True if success. False otherwise
 */
bool VideoProcessor::getNextFrame(cv::Mat &frame)
{
    return capture.read(frame);
}

/** 
 * writeNextFrame	-	to write the output frame
 *
 * @param frame	-	the frame to be written
 */
void VideoProcessor::writeNextFrame(cv::Mat &frame)
{
    if (extension.length()) { // then we write images

        std::stringstream ss;
        ss << outputFile << std::setfill('0') << std::setw(digits) << curIndex++ << extension;
        cv::imwrite(ss.str(),frame);

    } else { // then write video file

        writer.write(frame);
    }
}

/** 
 * playIt	-	play the frames of the sequence
 *
 */
void VideoProcessor::playIt()
{
    // current frame
    cv::Mat input;

    // if no capture device has been set
    if (!isOpened())
        return;

    // is playing
    stop = false;

    // update buttons
    emit updateBtn();

    while (!isStop()) {

        // read next frame if any
        if (!getNextFrame(input))
            break;

        curPos = capture.get(CV_CAP_PROP_POS_FRAMES);

        // display input frame
        emit showFrame(input);

        // update the progress bar
        emit updateProgressBar();

        // introduce a delay
        emit sleep(delay);
    }
    if (!isStop()){
        emit revert();
    }
}

/** 
 * pauseIt	-	pause playing
 *
 */
void VideoProcessor::pauseIt()
{
    stop = true;
    emit updateBtn();
}

/** 
 * motionMagnify	-	eulerian motion magnification
 *
 */
void VideoProcessor::motionMagnify()
{
    // create a temp file
    createTemp();

    // current frame
    cv::Mat input;
    // output frame
    cv::Mat output;

    std::vector<cv::Mat_<cv::Vec3f> > pyramid;
    std::vector<cv::Mat_<cv::Vec3f> > filtered;

    // if no capture device has been set
    if (!isOpened())
        return;

    // set the modify flag to be true
    modify = true;

    // is processing
    stop = false;

    // save the current position
    long pos = curPos;
    // jump to the first frame
    jumpTo(0);

    while (!isStop()) {

        // read next frame if any
        if (!getNextFrame(input))
            break;        

        cv::Mat_<cv::Vec3f> s;

        // 1. convert to ntsc color space
        rgb2ntsc(input, s);

        // 2. spatial filtering one frame
        spatialFilter(s, pyramid);

        // 3. temporal filtering a pyramid of one frame
        // and amplify the motion
        if (fnumber == 0){      // is first frame
            lowpass1 = pyramid;
            lowpass2 = pyramid;
            filtered = pyramid;
        } else {
            for (int i=0; i<levels; ++i) {
                curLevel = i;
                temporalFilter(pyramid.at(i), filtered.at(i));
            }

            // amplify each spatial frequency bands according to Figure 6 of paper
            cv::Mat_<cv::Vec3f> temp = filtered.at(0);
            int w = temp.size().width;
            int h = temp.size().height;

            delta = lambda_c/8.0/(1.0+alpha);
            // the factor to boost alpha above the bound
            // (for better visualization)
            exaggeration_factor = 2.0;

            // compute the representative wavelength lambda for the lowest spatial
            // frequency band of Laplacian pyramid
            lambda = sqrt(w*w + h*h)/3;  // 3 is experimental constant

            for (int i=0; i<levels; ++i) {
                curLevel = i;

                amplify(filtered.at(i));

                // go one level down on pyramid
                // representative lambda will reduce by factor of 2
                lambda /= 2.0;
            }
        }

        // 4. combine motion and image
        combineImageAndMotion(s, filtered, output);

        // 5. convert back to rgb color space and CV_8UC3
        s = output.clone();
        ntsc2rgb(s, s);
        output = s.clone();
        double minVal, maxVal;
        minMaxLoc(output, &minVal, &maxVal); //find minimum and maximum intensities
        output.convertTo(output, CV_8UC3, 255.0/(maxVal - minVal),
                  -minVal * 255.0/(maxVal - minVal));


        // write the frame to the temp file
        tempWriter.write(output);

        // update process
        std::string msg= "Processing...";
        emit updateProcessProgress(msg, floor((fnumber++) * 100.0 / length));
    }
    if (!isStop()){
        emit revert();
    }
    emit closeProgressDialog();

    // release the temp writer
    tempWriter.release();

    // change the video to the processed video 
    setInput(tempFile);

    // jump back to the original position
    jumpTo(pos);
}

/**
 * colorMagnify	-	color magnification
 *
 */
void VideoProcessor::colorMagnify()
{
    // create a temp file
    createTemp();

    // current frame
    cv::Mat input;
    // output frame
    cv::Mat output;
    // for down-sampling and up-sampling
    cv::Mat currentLevel;

    // down-sampled frames
    std::vector<cv::Mat_<cv::Vec3f> > frames;
    // concatenate image of all the down-sample frames
    cv::Mat_<cv::Vec3f> sequence;
    // concatenate filtered image
    cv::Mat_<cv::Vec3f> filtered;

    // if no capture device has been set
    if (!isOpened())
        return;

    // set the modify flag to be true
    modify = true;

    // is processing
    stop = false;

    // save the current position
    long pos = curPos;

    // jump to the first frame
    jumpTo(0);

    // spatial filtering
    // downsample each frame and form a sequence
    while (getNextFrame(input)){
        std::vector<cv::Mat_<cv::Vec3f> > pyramid;
        spatialFilter(input, pyramid);
        frames.push_back(pyramid.at(levels-1));
    }

    // load all the frames into a single large Mat
    // where each column is a reshaped single frame
    cv::Size frameSize = frames.at(0).size();
    cv::Mat_<cv::Vec3f> temp(frameSize.width*frameSize.height, length-1, CV_8UC3);
    for (int i = 0; i < length-1; ++i) {    // get a frame if any
        cv::Mat_<cv::Vec3f> input = frames.at(i);
        cv::Mat_<cv::Vec3f> reshaped = input.reshape(3, input.cols*input.rows).clone();
        cv::Mat_<cv::Vec3f> line = temp.col(i);
        reshaped.copyTo(line);
    }

    // convert to ntsc color space
    rgb2ntsc(temp.clone(), sequence);

    // temporal filtering
    temporalIdealFilter(sequence, filtered);

    // amplify color motion
    filtered *= alpha;

    // up-sample color motion
    currentLevel = filtered.clone();
    for (int i = 0; i < levels; ++i) {
        cv::Mat up;
        cv::pyrUp(currentLevel, up);
        currentLevel = up;
    }

    cv::imwrite("before.png", sequence);

    // combine color motion and source frame
    cv::resize(filtered, filtered, sequence.size());
    cv::imwrite("motion.png", filtered);
    sequence += filtered;
    ntsc2rgb(sequence, sequence);
    cv::imwrite("result.png", sequence);

    jumpTo(pos);
}


/** 
 * writeOutput	-	write the processed result
 *
 */
void VideoProcessor::writeOutput()
{
    cv::Mat input;

    // if no capture device has been set
    if (!isOpened() || !writer.isOpened())
        return;

    // save the current position
    long pos = curPos;
    
    // jump to the first frame
    jumpTo(0);

    while (getNextFrame(input)) {

        // write output sequence
        if (outputFile.length()!=0)
            writeNextFrame(input);
    }

    // set the modify flag to false
    modify = false;

    // release the writer
    writer.release();

    // jump back to the original position
    jumpTo(pos);
}

/** 
 * revertVideo	-	revert playing
 *
 */
void VideoProcessor::revertVideo()
{
    // pause the video
    jumpTo(0);    
    curPos = 0;
    pauseIt();
    emit updateProgressBar();
}